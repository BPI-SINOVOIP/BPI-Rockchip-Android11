#! /usr/bin/perl
#
# Copyright © 2017 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#

use strict;
use warnings;
use 5.010;

use Getopt::Std;

chomp(my $igt_root = `pwd -P`);
my $wsim = "$igt_root/benchmarks/gem_wsim";
my $wrk_root = "$igt_root/benchmarks/wsim";
my $tracepl = "$igt_root/scripts/trace.pl";
my $tolerance = 0.01;
my $client_target_s = 10;
my $idle_tolerance_pct = 2.0;
my $verbose = 0;
my $gt2 = 0;
my $show_cmds = 0;
my $realtime_target = 0;
my $wps_target = 0;
my $wps_target_param = 0;
my $multi_mode = 0;
my @multi_workloads;
my $w_direct;
my $balancer;
my $nop;
my %opts;

my @balancers = ( 'rr', 'rand', 'qd', 'qdr', 'qdavg', 'rt', 'rtr', 'rtavg',
		  'context', 'busy', 'busy-avg', 'i915' );
my %bal_skip_H = ( 'rr' => 1, 'rand' => 1, 'context' => 1, , 'busy' => 1,
		   'busy-avg' => 1, 'i915' => 1 );
my %bal_skip_R = ( 'i915' => 1 );
my %bal_skip_G = ( 'i915' => 1 );

my @workloads = (
	'media_load_balance_17i7.wsim',
	'media_load_balance_19.wsim',
	'media_load_balance_4k12u7.wsim',
	'media_load_balance_fhd26u7.wsim',
	'media_load_balance_hd01.wsim',
	'media_load_balance_hd06mp2.wsim',
	'media_load_balance_hd12.wsim',
	'media_load_balance_hd17i4.wsim',
	'media_1n2_480p.wsim',
	'media_1n3_480p.wsim',
	'media_1n4_480p.wsim',
	'media_1n5_480p.wsim',
	'media_1n2_asy.wsim',
	'media_1n3_asy.wsim',
	'media_1n4_asy.wsim',
	'media_1n5_asy.wsim',
	'media_mfe2_480p.wsim',
	'media_mfe3_480p.wsim',
	'media_mfe4_480p.wsim',
	'media_nn_1080p.wsim',
	'media_nn_480p.wsim',
    );

sub show_cmd
{
	my ($cmd) = @_;

	say "\n+++ $cmd" if $show_cmds;
}

sub calibrate_nop
{
	my ($delay, $nop);
	my $cmd = "$wsim";

	show_cmd($cmd);
	open WSIM, "$cmd |" or die;
	while (<WSIM>) {
		chomp;
		if (/Nop calibration for (\d+)us delay is (\d+)./) {
			$delay = $1;
			$nop = $2;
		}

	}
	close WSIM;

	die unless $nop;

	return $nop
}

sub can_balance_workload
{
	my ($wrk) = @_;
	my $res = 0;

	open WRK, "$wrk_root/$wrk" or die;
	while (<WRK>) {
		chomp;
		if (/\.VCS\./) {
			$res = 1;
			last;
		}
	}
	close WRK;

	return $res;
}

sub add_wps_arg
{
	my (@args) = @_;
	my $period;

	return @args if $realtime_target <= 0;

	$period = int(1000000 / $realtime_target);
	push @args, '-a';
	push @args, 'p.$period';

	return @args;
}

sub run_workload
{
	my (@args) = @_;
	my ($time, $wps, $cmd);
	my @ret;

	@args = add_wps_arg(@args);
	push @args, '-2' if $gt2;

	unshift @args, $wsim;
	$cmd = join ' ', @args;
	show_cmd($cmd);

	open WSIM, "$cmd |" or die;
	while (<WSIM>) {
		chomp;
		if (/^(\d+\.\d+)s elapsed \((\d+\.?\d+) workloads\/s\)$/) {
			$time = $1;
			$wps = $2;
		} elsif (/(\d+)\: \d+\.\d+s elapsed \(\d+ cycles, (\d+\.?\d+) workloads\/s\)/) {
			$ret[$1] = $2;
		}
	}
	close WSIM;

	return ($time, $wps, \@ret);
}

sub dump_cmd
{
	my ($cmd, $file) = @_;

	show_cmd("$cmd > $file");

	open FOUT, '>', $file or die;
	open TIN, "$cmd |" or die;
	while (<TIN>) {
		print FOUT $_;
	}
	close TIN;
	close FOUT;
}

sub trace_workload
{
	my ($wrk, $b, $r, $c) = @_;
	my @args = ($tracepl, '--trace', $wsim, '-q', '-n', $nop, '-r', $r, '-c', $c);
	my $min_batches = 16 + $r * $c / 2;
	my @skip_engine;
	my %engines;
	my ($cmd, $file);

	push @args, '-2' if $gt2;

	unless ($b eq '<none>') {
		push @args, '-R';
		push @args, split /\s+/, $b;
	}

	if (defined $w_direct) {
		push @args, split /\s+/, $wrk;
	} else {
		push @args, '-w';
		push @args, $wrk_root . '/' . $wrk;
	}

	show_cmd(join ' ', @args);
	if (-e 'perf.data') {
		unlink 'perf.data' or die;
	}
	system(@args) == 0 or die;

	$cmd = "perf script | $tracepl";
	show_cmd($cmd);
	open CMD, "$cmd |" or die;
	while (<CMD>) {
		chomp;
		if (/Ring(\S+): (\d+) batches.*?(\d+\.?\d+)% idle,/) {
			if ($2 >= $min_batches) {
				$engines{$1} = $3;
			} else {
				push @skip_engine, $1;
			}
		} elsif (/GPU: (\d+\.?\d+)% idle/) {
			$engines{'gpu'} = $1;
		}
	}
	close CMD;

	$wrk =~ s/$wrk_root//g;
	$wrk =~ s/\.wsim//g;
	$wrk =~ s/-w/W/g;
	$wrk =~ s/[ -]/_/g;
	$wrk =~ s/\//-/g;
	$b =~ s/[ <>]/_/g;
	$file = "${wrk}_${b}_-r${r}_-c${c}";

	dump_cmd('perf script', "${file}.trace");

	$cmd = "perf script | $tracepl --html -x ctxsave -s -c ";
	$cmd .= join ' ', map("-i $_", @skip_engine);

	dump_cmd($cmd, "${file}.html");

	return \%engines;
}

sub calibrate_workload
{
	my ($wrk) = @_;
	my $tol = $tolerance;
	my $loops = 0;
	my $error;
	my $r;

	$r = $realtime_target > 0 ? $realtime_target * $client_target_s : 23;
	for (;;) {
		my @args = ('-n', $nop, '-r', $r);
		my ($time, $wps);

		if (defined $w_direct) {
			push @args, split /\s+/, $wrk;
		} else {
			push @args, '-w';
			push @args, $wrk_root . '/' . $wrk;
		}

		($time, $wps) = run_workload(@args);

		$wps = $r / $time if $w_direct;
		$error = abs($time - $client_target_s) / $client_target_s;

		last if $error <= $tol;

		$r = int($wps * $client_target_s);
		$loops = $loops + 1;
		if ($loops >= 3) {
			$tol = $tol * (1.2 + ($tol));
			$loops = 0;
		}
		last if $tol > 0.2;
	}

	return ($r, $error);
}

sub find_saturation_point
{
	my ($wrk, $rr, $verbose, @args) = @_;
	my ($last_wps, $c, $swps, $wwps);
	my $target = $realtime_target > 0 ? $realtime_target : $wps_target;
	my $r = $rr;
	my $wcnt;
	my $maxc;
	my $max = 0;

	push @args, '-v' if $multi_mode and $w_direct;

	if (defined $w_direct) {
		push @args, split /\s+/, $wrk;
		$wcnt = () = $wrk =~ /-[wW]/gi;

	} else {
		push @args, '-w';
		push @args, $wrk_root . '/' . $wrk;
		$wcnt = 1;
	}

	for ($c = 1; ; $c = $c + 1) {
		my ($time, $wps);
		my @args_ = (@args, ('-r', $r, '-c', $c));

		($time, $wps, $wwps) = run_workload(@args_);

		say "        $c clients is $wps wps." if $verbose;

		if ($c > 1) {
			my $delta;

			if ($target <= 0) {
				if ($wps > $max) {
					$max = $wps;
					$maxc = $c;
				}
				$delta = ($wps - $last_wps) / $last_wps;
				if ($delta > 0) {
					last if $delta < $tolerance;
				} else {
					$delta = ($wps - $max) / $max;
					last if abs($delta) >= $tolerance;
				}
			} else {
				$delta = ($wps / $c - $target) / $target;
				last if $delta < 0 and abs($delta) >= $tolerance;
			}
			$r = int($rr * ($client_target_s / $time));
		} elsif ($c == 1) {
			$swps = $wps;
			return ($c, $wps, $swps, $wwps) if $wcnt > 1 or
							   $multi_mode or
							   ($wps_target_param < 0 and
							    $wps_target == 0);
		}

		$last_wps = $wps;
	}

	if ($target <= 0) {
		return ($maxc, $max, $swps, $wwps);
	} else {
		return ($c - 1, $last_wps, $swps, $wwps);
	}
}

getopts('hv2xmn:b:W:B:r:t:i:R:T:w:', \%opts);

if (defined $opts{'h'}) {
	print <<ENDHELP;
Supported options:

  -h          Help text.
  -v          Be verbose.
  -x          Show external commands.
  -2          Run gem_wsim in GT2 mode.
  -n num      Nop calibration.
  -b str      Balancer to pre-select.
              Skips balancer auto-selection.
              Passed straight the gem_wsim so use like -b "-b qd -R"
  -W a,b,c    Override the default list of workloads.
  -B a,b,c    Override the default list of balancers.
  -r sec      Target workload duration.
  -t pct      Calibration tolerance.
  -i pct      Engine idleness tolerance.
  -R wps      Run workloads in the real-time mode at wps rate.
  -T wps      Calibrate up to wps/client target instead of GPU saturation.
              Negative values set the target based on the single client
              performance where target = single-client-wps / -N.
  -w str      Pass-through to gem_wsim. Overrides normal workload selection.
  -m          Multi-workload mode. All selected workloads will be run in
              parallel and overal score will be relative to when run
              individually.
ENDHELP
	exit 0;
}

$verbose = 1 if defined $opts{'v'};
$gt2 = 1 if defined $opts{'2'};
$show_cmds = 1 if defined $opts{'x'};
$multi_mode = 1 if defined $opts{'m'};
if (defined $opts{'b'}) {
	die unless substr($opts{'b'}, 0, 2) eq '-b';
	$balancer = $opts{'b'};
}
if (defined $opts{'B'}) {
	@balancers = split /,/, $opts{'B'};
} else {
	unshift @balancers, '';
}
@workloads = split /,/, $opts{'W'} if defined $opts{'W'};
$client_target_s = $opts{'r'} if defined $opts{'r'};
$tolerance = $opts{'t'} / 100.0 if defined $opts{'t'};
$idle_tolerance_pct = $opts{'i'} if defined $opts{'i'};
$realtime_target = $opts{'R'} if defined $opts{'R'};
$wps_target = $opts{'T'} if defined $opts{'T'};
$wps_target_param = $wps_target;
$w_direct = $opts{'w'} if defined $opts{'w'};

if ($multi_mode) {
	die if $w_direct; # Not supported
	@multi_workloads = @workloads;
}

@workloads = ($w_direct) if defined $w_direct;

say "Workloads:";
print map { "  $_\n" } @workloads;
print "Balancers: ";
say map { "$_," } @balancers;
say "Target workload duration is ${client_target_s}s.";
say "Calibration tolerance is $tolerance.";
say "Real-time mode at ${realtime_target} wps." if $realtime_target > 0;
say "Wps target is ${wps_target} wps." if $wps_target > 0;
say "Multi-workload mode." if $multi_mode;
$nop = $opts{'n'};
$nop = calibrate_nop() unless $nop;
say "Nop calibration is $nop.";

goto VERIFY if defined $balancer;

my (%best_bal, %best_bid);
my %results;
my %scores;
my %wscores;
my %cscores;
my %cwscores;
my %mscores;
my %mwscores;

sub add_points
{
	my ($wps, $scores, $wscores) = @_;
	my ($min, $max, $spread);
	my @sorted;

	@sorted = sort { $b <=> $a } values %{$wps};
	$max = $sorted[0];
	$min = $sorted[-1];
	$spread = $max - $min;
	die if $spread < 0;

	foreach my $w (keys %{$wps}) {
		my ($score, $wscore);

		unless (exists $scores->{$w}) {
			$scores->{$w} = 0;
			$wscores->{$w} = 0;
		}

		$score = $wps->{$w} / $max;
		$scores->{$w} = $scores->{$w} + $score;
		$wscore = $score * $spread / $max;
		$wscores->{$w} = $wscores->{$w} + $wscore;
	}
}

my @saturation_workloads = $multi_mode ? @multi_workloads : @workloads;
my %allwps;
my $widx = 0;

push @saturation_workloads, '-w ' . join ' -w ', map("$wrk_root/$_", @workloads)
     if $multi_mode;

foreach my $wrk (@saturation_workloads) {
	my @args = ( "-n $nop");
	my ($r, $error, $should_b, $best);
	my (%wps, %cwps, %mwps);
	my @sorted;
	my $range;

	$w_direct = $wrk if $multi_mode and $widx == $#saturation_workloads;

	$should_b = 1;
	$should_b = can_balance_workload($wrk) unless defined $w_direct;

	print "\nEvaluating '$wrk'...";

	($r, $error) = calibrate_workload($wrk);
	say " ${client_target_s}s is $r workloads. (error=$error)";

	say "  Finding saturation points for '$wrk'...";

	BAL: foreach my $bal (@balancers) {
		GBAL: foreach my $G ('', '-G', '-d', '-G -d') {
			foreach my $H ('', '-H') {
				my @xargs;
				my ($w, $c, $s, $bwwps);
				my $bid;

				if ($bal ne '') {
					next GBAL if $G =~ '-G' and exists $bal_skip_G{$bal};

					push @xargs, "-b $bal";
					push @xargs, '-R' unless exists $bal_skip_R{$bal};
					push @xargs, $G if $G ne '';
					push @xargs, $H if $H ne '';
					$bid = join ' ', @xargs;
					print "    $bal balancer ('$bid'): ";
				} else {
					$bid = '<none>';
					print "    No balancing: ";
				}

				$wps_target = 0 if $wps_target_param < 0;

				($c, $w, $s, $bwwps) =
					find_saturation_point($wrk, $r, 0,
							      (@args, @xargs));

				if ($wps_target_param < 0) {
					$wps_target = $s / -$wps_target_param;

					($c, $w, $s, $bwwps) =
						find_saturation_point($wrk, $r,
								      0,
								      (@args,
								       @xargs));
				}

				if ($multi_mode and $w_direct) {
					my $widx;

					die unless scalar(@multi_workloads) ==
						   scalar(@{$bwwps});
					die unless scalar(@multi_workloads) ==
						   scalar(keys %allwps);

					# Total of all workload wps from the
					# mixed run.
					$w = 0;
					foreach $widx (0..$#{$bwwps}) {
						$w += $bwwps->[$widx];
					}

					# Total of all workload wps from when
					# ran individually with the best
					# balancer.
					my $tot = 0;
					foreach my $wrk (@multi_workloads) {
						$tot += $allwps{$wrk}->{$best_bid{$wrk}};
					}

					# Normalize mixed sum with sum of
					# individual runs.
					$w *= 100;
					$w /= $tot;

					# Second metric is average of each
					# workload wps normalized by their
					# individual run performance with the
					# best balancer.
					$s = 0;
					$widx = 0;
					foreach my $wrk (@multi_workloads) {
						$s += 100 * $bwwps->[$widx] /
						      $allwps{$wrk}->{$best_bid{$wrk}};
						$widx++;
					}
					$s /= scalar(@multi_workloads);

					say sprintf('Aggregate (normalized) %.2f%%; fairness %.2f%%',
						    $w, $s);
				} else {
					$allwps{$wrk} = \%wps;
				}

				$wps{$bid} = $w;
				$cwps{$bid} = $s;

				if ($realtime_target > 0 || $wps_target_param > 0) {
					$mwps{$bid} = $w * $c;
				} else {
					$mwps{$bid} = $w + $s;
				}

				say "$c clients ($w wps, $s wps single client, score=$mwps{$bid})."
				    unless $multi_mode and $w_direct;

				last BAL unless $should_b;
				next BAL if $bal eq '';
				next GBAL if exists $bal_skip_H{$bal};
			}
		}
	}

	$widx++;

	@sorted = sort { $mwps{$b} <=> $mwps{$a} } keys %mwps;
	$best_bid{$wrk} = $sorted[0];
	@sorted = sort { $b <=> $a } values %mwps;
	$range = 1 - $sorted[-1] / $sorted[0];
	$best_bal{$wrk} = $sorted[0];

	next if $multi_mode and not $w_direct;

	say "  Best balancer is '$best_bid{$wrk}' (range=$range).";


	$results{$wrk} = \%mwps;

	add_points(\%wps, \%scores, \%wscores);
	add_points(\%mwps, \%mscores, \%mwscores);
	add_points(\%cwps, \%cscores, \%cwscores);
}

sub dump_scoreboard
{
	my ($n, $h) = @_;
	my ($i, $str, $balancer);
	my ($max, $range);
	my @sorted;

	@sorted = sort { $b <=> $a } values %{$h};
	$max = $sorted[0];
	$range = 1 - $sorted[-1] / $max;
	$str = "$n rank (range=$range):";
	say "\n$str";
	say '=' x length($str);
	$i = 1;
	foreach my $w (sort { $h->{$b} <=> $h->{$a} } keys %{$h}) {
		my $score;

		$balancer = $w if $i == 1;
		$score = $h->{$w} / $max;

		say "  $i: '$w' ($score)";

		$i = $i + 1;
	}

	return $balancer;
}

dump_scoreboard($multi_mode ? 'Throughput' : 'Total wps', \%scores);
dump_scoreboard('Total weighted wps', \%wscores) unless $multi_mode;
dump_scoreboard($multi_mode ? 'Fairness' : 'Per client wps', \%cscores);
dump_scoreboard('Per client weighted wps', \%cwscores) unless $multi_mode;
$balancer = dump_scoreboard($multi_mode ? 'Combined' : 'Combined wps', \%mscores);
$balancer = dump_scoreboard('Combined weighted wps', \%mwscores) unless $multi_mode;

VERIFY:

my %problem_wrk;

die unless defined $balancer;

say "\nBalancer is '$balancer'.";
say "Idleness tolerance is $idle_tolerance_pct%.";

if ($multi_mode) {
	$w_direct = '-w ' . join ' -w ', map("$wrk_root/$_", @workloads);
	@workloads = ($w_direct);
}

foreach my $wrk (@workloads) {
	my @args = ( "-n $nop" );
	my ($r, $error, $c, $wps, $swps);
	my $saturated = 0;
	my $result = 'Pass';
	my $vcs2 = $gt2 ? '1:0' : '2:1';
	my %problem;
	my $engines;

	next if not defined $w_direct and not can_balance_workload($wrk);

	push @args, $balancer unless $balancer eq '<none>';

	if (scalar(keys %results)) {
		$r = $results{$wrk}->{$balancer} / $best_bal{$wrk} * 100.0;
	} else {
		$r = '---';
	}
	say "  \nProfiling '$wrk' ($r% of best)...";

	($r, $error) = calibrate_workload($wrk);
	say "      ${client_target_s}s is $r workloads. (error=$error)";

	($c, $wps, $swps) = find_saturation_point($wrk, $r, $verbose, @args);
	say "      Saturation at $c clients ($wps workloads/s).";
	push @args, "-c $c";

	$engines = trace_workload($wrk, $balancer, $r, $c);

	foreach my $key (keys %{$engines}) {
		next if $key eq 'gpu';
		$saturated = $saturated + 1
			     if $engines->{$key} < $idle_tolerance_pct;
	}

	if ($saturated == 0) {
		# Not a single saturated engine
		$result = 'FAIL';
	} elsif (not exists $engines->{'2:0'} or not exists $engines->{$vcs2}) {
		# VCS1 and VCS2 not present in a balancing workload
		$result = 'FAIL';
	} elsif ($saturated == 1 and
		 ($engines->{'2:0'} < $idle_tolerance_pct or
		  $engines->{$vcs2} < $idle_tolerance_pct)) {
		# Only one VCS saturated
		$result = 'WARN';
	}

	$result = 'WARN' if $engines->{'gpu'} > $idle_tolerance_pct;

	if ($result ne 'Pass') {
		$problem{'c'} = $c;
		$problem{'r'} = $r;
		$problem{'stats'} = $engines;
		$problem_wrk{$wrk} = \%problem;
	}

	print "    $result [";
	print map " $_: $engines->{$_}%,", sort keys %{$engines};
	say " ]";
}

say "\nProblematic workloads were:" if scalar(keys %problem_wrk) > 0;
foreach my $wrk (sort keys %problem_wrk) {
	my $problem = $problem_wrk{$wrk};

	print "   $wrk -c $problem->{'c'} -r $problem->{'r'} [";
	print map " $_: $problem->{'stats'}->{$_}%,",
	      sort keys %{$problem->{'stats'}};
	say " ]";
}
