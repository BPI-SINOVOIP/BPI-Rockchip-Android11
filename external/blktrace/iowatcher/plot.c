/*
 * Copyright (C) 2012 Fusion-io
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License v2 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 *  Parts of this file were imported from Jens Axboe's blktrace sources (also GPL)
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <inttypes.h>
#include <string.h>
#include <asm/types.h>
#include <errno.h>
#include <sys/mman.h>
#include <time.h>
#include <math.h>

#include "plot.h"

static int io_graph_scale = 8;
static int graph_width = 700;
static int graph_height = 250;
static int graph_circle_extra = 30;
static int graph_inner_x_margin = 2;
static int graph_inner_y_margin = 2;
static int graph_tick_len = 5;
static int graph_left_pad = 120;
static int tick_label_pad = 16;
static int tick_font_size = 15;
static char *font_family = "sans-serif";

/* this is the title for the whole page */
static int plot_title_height = 50;
static int plot_title_font_size = 25;

/* this is the label at the top of each plot */
static int plot_label_height = 60;
static int plot_label_font_size = 20;

/* label for each axis is slightly smaller */
static int axis_label_font_size = 16;

int legend_x_off = 45;
int legend_y_off = -10;
int legend_font_size = 15;
int legend_width = 80;

static int rolling_avg_secs = 0;

static int line_len = 1024;
static char line[1024];

static int final_height = 0;
static int final_width = 0;

static char *colors[] = {
	"blue", "darkgreen",
	"red",
	"darkviolet",
	"orange",
	"aqua",
	"brown", "#00FF00",
	"yellow", "coral",
	"black", "darkred",
	"fuchsia", "crimson",
	NULL };

extern unsigned int longest_proc_name;

char *pick_color(void)
{
	static int color_index;
	char *ret = colors[color_index];

	if (!ret) {
		color_index = 0;
		ret = colors[color_index];
	}
	color_index++;
	return ret;
}

char *pick_fio_color(void)
{
	static int fio_color_index;
	char *ret = colors[fio_color_index];

	if (!ret) {
		fio_color_index = 0;
		ret = colors[fio_color_index];
	}
	fio_color_index += 2;
	return ret;
}

static int cpu_color_index;

char *pick_cpu_color(void)
{
	char *ret = colors[cpu_color_index];
	if (!ret) {
		cpu_color_index = 0;
		ret = colors[cpu_color_index];
	}
	cpu_color_index++;
	return ret;
}

void reset_cpu_color(void)
{
	cpu_color_index = 0;
}

struct graph_line_data *alloc_line_data(unsigned int min_seconds,
					unsigned int max_seconds,
					unsigned int stop_seconds)
{
	int size = sizeof(struct graph_line_data) + (stop_seconds + 1) * sizeof(struct graph_line_pair);
	struct graph_line_data *gld;

	gld = calloc(1, size);
	if (!gld) {
		fprintf(stderr, "Unable to allocate memory for graph data\n");
		exit(1);
	}
	gld->min_seconds = min_seconds;
	gld->max_seconds = max_seconds;
	gld->stop_seconds = stop_seconds;
	return gld;
}

struct graph_dot_data *alloc_dot_data(unsigned int min_seconds,
				      unsigned int max_seconds,
				      u64 min_offset, u64 max_offset,
				      unsigned int stop_seconds,
				      char *color, char *label)
{
	int size;
	int arr_size;
	int rows = graph_height * io_graph_scale;
	int cols = graph_width;
	struct graph_dot_data *gdd;

	size = sizeof(struct graph_dot_data);

	/* the number of bits */
	arr_size = (rows + 1) * cols;

	/* the number of bytes */
	arr_size = (arr_size + 7) / 8;

	gdd = calloc(1, size + arr_size);
	if (!gdd) {
		fprintf(stderr, "Unable to allocate memory for graph data\n");
		exit(1);
	}
	gdd->min_seconds = min_seconds;
	gdd->max_seconds = max_seconds;
	gdd->stop_seconds = stop_seconds;
	gdd->rows = rows;
	gdd->cols = cols;
	gdd->min_offset = min_offset;
	gdd->max_offset = max_offset;
	gdd->color = color;
	gdd->label = label;

	if (strlen(label) > longest_proc_name)
		longest_proc_name = strlen(label);

	return gdd;
}

void set_gdd_bit(struct graph_dot_data *gdd, u64 offset, double bytes, double time)
{
	double bytes_per_row = (double)(gdd->max_offset - gdd->min_offset + 1) / gdd->rows;
	double secs_per_col = (double)(gdd->max_seconds - gdd->min_seconds) / gdd->cols;
	double col;
	double row;
	int col_int;
	int row_int;
	int bit_index;
	int arr_index;
	int bit_mod;
	double mod = bytes_per_row;

	if (offset > gdd->max_offset || offset < gdd->min_offset)
		return;
	time = time / 1000000000.0;
	if (time < gdd->min_seconds || time > gdd->max_seconds)
		return;
	gdd->total_ios++;
	while (bytes > 0 && offset <= gdd->max_offset) {
		row = (double)(offset - gdd->min_offset) / bytes_per_row;
		col = (time - gdd->min_seconds) / secs_per_col;

		col_int = floor(col);
		row_int = floor(row);
		bit_index = row_int * gdd->cols + col_int;
		arr_index = bit_index / 8;
		bit_mod = bit_index % 8;

		gdd->data[arr_index] |= 1 << bit_mod;
		offset += mod;
		bytes -= mod;
	}
}

static double rolling_avg(struct graph_line_pair *data, int index, int distance)
{
	double sum = 0;
	int start;

	if (distance < 0)
		distance = 1;
	if (distance > index) {
		start = 0;
	} else {
		start = index - distance;
	}
	distance = 0;
	while (start <= index) {
		double avg;

		if (data[start].count)
			avg = ((double)data[start].sum) / data[start].count;
		else
			avg= 0;

		sum += avg;
		distance++;
		start++;
	}
	return sum / distance;
}

static void write_check(int fd, char *buf, size_t size)
{
	ssize_t ret;

	ret = write(fd, buf, size);
	if (ret != (ssize_t)size) {
		if (ret < 0)
			perror("write failed");
		else
			fprintf(stderr, "error: short write\n");
		exit(1);
	}
}

void write_svg_header(int fd)
{
	char *spaces = "                                                    \n";
	char *header = "<svg  xmlns=\"http://www.w3.org/2000/svg\">\n";
	char *filter1 ="<filter id=\"shadow\">\n "
		"<feOffset result=\"offOut\" in=\"SourceAlpha\" dx=\"4\" dy=\"4\" />\n "
		"<feGaussianBlur result=\"blurOut\" in=\"offOut\" stdDeviation=\"2\" />\n "
		"<feBlend in=\"SourceGraphic\" in2=\"blurOut\" mode=\"normal\" />\n "
		"</filter>\n";
	char *filter2 ="<filter id=\"textshadow\" x=\"0\" y=\"0\" width=\"200%\" height=\"200%\">\n "
		"<feOffset result=\"offOut\" in=\"SourceAlpha\" dx=\"1\" dy=\"1\" />\n "
		"<feGaussianBlur result=\"blurOut\" in=\"offOut\" stdDeviation=\"1.5\" />\n "
		"<feBlend in=\"SourceGraphic\" in2=\"blurOut\" mode=\"normal\" />\n "
		"</filter>\n";
	char *filter3 ="<filter id=\"labelshadow\" x=\"0\" y=\"0\" width=\"200%\" height=\"200%\">\n "
		"<feOffset result=\"offOut\" in=\"SourceGraphic\" dx=\"3\" dy=\"3\" />\n "
		"<feColorMatrix result=\"matrixOut\" in=\"offOut\" type=\"matrix\" "
		"values=\"0.2 0 0 0 0 0 0.2 0 0 0 0 0 0.2 0 0 0 0 0 1 0\" /> "
		"<feGaussianBlur result=\"blurOut\" in=\"offOut\" stdDeviation=\"2\" />\n "
		"<feBlend in=\"SourceGraphic\" in2=\"blurOut\" mode=\"normal\" />\n "
		"</filter>\n";
	char *defs_start = "<defs>\n";
	char *defs_close = "</defs>\n";
	final_width = 0;
	final_height = 0;

	write_check(fd, header, strlen(header));
	/* write a bunch of spaces so we can stuff in the width and height later */
	write_check(fd, spaces, strlen(spaces));
	write_check(fd, spaces, strlen(spaces));
	write_check(fd, spaces, strlen(spaces));

	write_check(fd, defs_start, strlen(defs_start));
	write_check(fd, filter1, strlen(filter1));
	write_check(fd, filter2, strlen(filter2));
	write_check(fd, filter3, strlen(filter3));
	write_check(fd, defs_close, strlen(defs_close));
}

/* svg y offset for the traditional 0,0 (bottom left corner) of the plot */
static int axis_y(void)
{
	return plot_label_height + graph_height + graph_inner_y_margin;
}

/* this gives you the correct pixel for a given offset from the bottom left y axis */
static double axis_y_off_double(double y)
{
	return plot_label_height + graph_height - y;
}

static int axis_y_off(int y)
{
	return axis_y_off_double(y);
}

/* svg x axis offset from 0 */
static int axis_x(void)
{
	return graph_left_pad;
}

/* the correct pixel for a given X offset */
static double axis_x_off_double(double x)
{
	return graph_left_pad + graph_inner_x_margin + x;
}

static int axis_x_off(int x)
{
	return (int)axis_x_off_double(x);
}

/*
 * this draws a backing rectangle for the plot and it
 * also creates a new svg element so our offsets can
 * be relative to this one plot.
 */
void setup_axis(struct plot *plot)
{
	int len;
	int fd = plot->fd;
	int bump_height = tick_font_size * 3 + axis_label_font_size;
	int local_legend_width = legend_width;

	if (plot->no_legend)
		local_legend_width = 0;

	plot->total_width = axis_x_off(graph_width) + graph_left_pad / 2 + local_legend_width;
	plot->total_height = axis_y() + tick_label_pad + tick_font_size;

	if (plot->add_xlabel)
		plot->total_height += bump_height;

	/* backing rect */
	snprintf(line, line_len, "<rect x=\"%d\" y=\"%d\" width=\"%d\" "
		 "height=\"%d\" fill=\"white\" stroke=\"none\"/>",
		 plot->start_x_offset,
		plot->start_y_offset, plot->total_width + 40,
		plot->total_height + 20);
	len = strlen(line);
	write_check(fd, line, len);

	snprintf(line, line_len, "<rect x=\"%d\" y=\"%d\" width=\"%d\" "
		 "filter=\"url(#shadow)\" "
		 "height=\"%d\" fill=\"white\" stroke=\"none\"/>",
		 plot->start_x_offset + 15,
		plot->start_y_offset, plot->total_width, plot->total_height);
	len = strlen(line);
	write_check(fd, line, len);
	plot->total_height += 20;
	plot->total_width += 20;

	if (plot->total_height + plot->start_y_offset > final_height)
		final_height = plot->total_height + plot->start_y_offset;
	if (plot->start_x_offset + plot->total_width + 40 > final_width)
		final_width = plot->start_x_offset + plot->total_width + 40;

	/* create an svg object for all our coords to be relative against */
	snprintf(line, line_len, "<svg x=\"%d\" y=\"%d\">\n", plot->start_x_offset, plot->start_y_offset);
	write_check(fd, line, strlen(line));

	snprintf(line, 1024, "<path d=\"M%d %d h %d V %d H %d Z\" stroke=\"black\" stroke-width=\"2\" fill=\"none\"/>\n",
		 axis_x(), axis_y(),
		 graph_width + graph_inner_x_margin * 2, axis_y_off(graph_height) - graph_inner_y_margin,
		 axis_x());
	len = strlen(line);
	write_check(fd, line, len);
}

/*
 * this draws a backing rectangle for the plot and it
 * also creates a new svg element so our offsets can
 * be relative to this one plot.
 */
void setup_axis_spindle(struct plot *plot)
{
	int len;
	int fd = plot->fd;
	int bump_height = tick_font_size * 3 + axis_label_font_size;

	legend_x_off = -60;

	plot->total_width = axis_x_off(graph_width) + legend_width;
	plot->total_height = axis_y() + tick_label_pad + tick_font_size;

	if (plot->add_xlabel)
		plot->total_height += bump_height;

	/* backing rect */
	snprintf(line, line_len, "<rect x=\"%d\" y=\"%d\" width=\"%d\" "
		 "height=\"%d\" fill=\"white\" stroke=\"none\"/>",
		 plot->start_x_offset,
		plot->start_y_offset, plot->total_width + 10,
		plot->total_height + 20);
	len = strlen(line);
	write_check(fd, line, len);

	snprintf(line, line_len, "<rect x=\"%d\" y=\"%d\" width=\"%d\" "
		 "filter=\"url(#shadow)\" "
		 "height=\"%d\" fill=\"white\" stroke=\"none\"/>",
		 plot->start_x_offset + 15,
		plot->start_y_offset, plot->total_width - 30,
		plot->total_height);
	len = strlen(line);
	write_check(fd, line, len);
	plot->total_height += 20;

	if (plot->total_height + plot->start_y_offset > final_height)
		final_height = plot->total_height + plot->start_y_offset;
	if (plot->start_x_offset + plot->total_width + 40 > final_width)
		final_width = plot->start_x_offset + plot->total_width + 40;

	/* create an svg object for all our coords to be relative against */
	snprintf(line, line_len, "<svg x=\"%d\" y=\"%d\">\n", plot->start_x_offset, plot->start_y_offset);
	write_check(fd, line, strlen(line));

}

/* draw a plot title.  This should be done only once,
 * and it bumps the plot width/height numbers by
 * what it draws.
 *
 * Call this before setting up the first axis
 */
void set_plot_title(struct plot *plot, char *title)
{
	int len;
	int fd = plot->fd;

	plot->total_height = plot_title_height;
	plot->total_width = axis_x_off(graph_width) + graph_left_pad / 2 + legend_width;

	/* backing rect */
	snprintf(line, line_len, "<rect x=\"0\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"white\" stroke=\"none\"/>",
		plot->start_y_offset, plot->total_width + 40, plot_title_height + 20);
	len = strlen(line);
	write_check(fd, line, len);

	snprintf(line, line_len, "<text x=\"%d\" y=\"%d\" font-family=\"%s\" font-size=\"%d\" "
		 "font-weight=\"bold\" fill=\"black\" style=\"text-anchor: %s\">%s</text>\n",
		 axis_x_off(graph_width / 2),
		plot->start_y_offset + plot_title_height / 2,
		font_family, plot_title_font_size, "middle", title);
	plot->start_y_offset += plot_title_height;
	len = strlen(line);
	write_check(fd, line, len);
}

#define TICK_MINI_STEPS 3

static double find_step(double first, double last, int num_ticks)
{
	int mini_step[TICK_MINI_STEPS] = { 1, 2, 5 };
	int cur_mini_step = 0;
	double step = (last - first) / num_ticks;
	double log10 = log(10);

	/* Round to power of 10 */
	step = exp(floor(log(step) / log10) * log10);
	/* Scale down step to provide enough ticks */
	while (cur_mini_step < TICK_MINI_STEPS
	       && (last - first) / (step * mini_step[cur_mini_step]) > num_ticks)
		cur_mini_step++;

	if (cur_mini_step > 0)
		step *= mini_step[cur_mini_step - 1];

	return step;
}

/*
 * create evenly spread out ticks along the xaxis.  if tick only is set
 * this just makes the ticks, otherwise it labels each tick as it goes
 */
void set_xticks(struct plot *plot, int num_ticks, int first, int last)
{
	int pixels_per_tick;
	double step;
	int i;
	int tick_y = axis_y_off(graph_tick_len) + graph_inner_y_margin;
	int tick_x = axis_x();
	int tick_only = plot->add_xlabel == 0;

	int text_y = axis_y() + tick_label_pad;

	char *middle = "middle";
	char *start = "start";

	step = find_step(first, last, num_ticks);
	/*
	 * We don't want last two ticks to be too close together so subtract
	 * 20% of the step from the interval
	 */
	num_ticks = (double)(last - first - step) / step + 1;
	pixels_per_tick = graph_width * step / (double)(last - first);

	for (i = 0; i < num_ticks; i++) {
		char *anchor;
		if (i != 0) {
			snprintf(line, line_len, "<rect x=\"%d\" y=\"%d\" width=\"2\" height=\"%d\" style=\"stroke:none;fill:black;\"/>\n",
				tick_x, tick_y, graph_tick_len);
			write_check(plot->fd, line, strlen(line));
			anchor = middle;
		} else {
			anchor = start;
		}

		if (!tick_only) {
			if (step >= 1)
				snprintf(line, line_len, "<text x=\"%d\" y=\"%d\" font-family=\"%s\" font-size=\"%d\" "
					"fill=\"black\" style=\"text-anchor: %s\">%d</text>\n",
					tick_x, text_y, font_family, tick_font_size, anchor,
					(int)(first + step * i));
			else
				snprintf(line, line_len, "<text x=\"%d\" y=\"%d\" font-family=\"%s\" font-size=\"%d\" "
					"fill=\"black\" style=\"text-anchor: %s\">%.2f</text>\n",
					tick_x, text_y, font_family, tick_font_size, anchor,
					first + step * i);
			write_check(plot->fd, line, strlen(line));
		}
		tick_x += pixels_per_tick;
	}

	if (!tick_only) {
		if (step >= 1)
			snprintf(line, line_len, "<text x=\"%d\" y=\"%d\" font-family=\"%s\" font-size=\"%d\" "
				"fill=\"black\" style=\"text-anchor: middle\">%d</text>\n",
				axis_x_off(graph_width - 2),
				text_y, font_family, tick_font_size, last);
		else
			snprintf(line, line_len, "<text x=\"%d\" y=\"%d\" font-family=\"%s\" font-size=\"%d\" "
				"fill=\"black\" style=\"text-anchor: middle\">%.2f</text>\n",
				axis_x_off(graph_width - 2),
				text_y, font_family, tick_font_size, (double)last);
		write_check(plot->fd, line, strlen(line));
	}
}

void set_ylabel(struct plot *plot, char *label)
{
	int len;
	int fd = plot->fd;

	snprintf(line, line_len, "<text x=\"%d\" y=\"%d\" font-family=\"%s\" "
		 "transform=\"rotate(-90 %d %d)\" font-weight=\"bold\" "
		 "font-size=\"%d\" fill=\"black\" style=\"text-anchor: %s\">%s</text>\n",
		 graph_left_pad / 2 - axis_label_font_size,
		 axis_y_off(graph_height / 2),
		 font_family,
		 graph_left_pad / 2 - axis_label_font_size,
		 (int)axis_y_off(graph_height / 2),
		 axis_label_font_size, "middle", label);
	len = strlen(line);
	write_check(fd, line, len);
}

void set_xlabel(struct plot *plot, char *label)
{
	int len;
	int fd = plot->fd;
	snprintf(line, line_len, "<text x=\"%d\" y=\"%d\" font-family=\"%s\" "
		 "font-weight=\"bold\" "
		 "font-size=\"%d\" fill=\"black\" style=\"text-anchor: %s\">%s</text>\n",
		 axis_x_off(graph_width / 2),
		 axis_y() + tick_font_size * 3 + axis_label_font_size / 2,
		 font_family,
		 axis_label_font_size, "middle", label);
	len = strlen(line);
	write_check(fd, line, len);

}

/*
 * create evenly spread out ticks along the y axis.
 * The ticks are labeled as it goes
 */
void set_yticks(struct plot *plot, int num_ticks, int first, int last, char *units)
{
	int pixels_per_tick = graph_height / num_ticks;
	int step = (last - first) / num_ticks;
	int i;
	int tick_y = 0;
	int text_x = axis_x() - 6;
	int tick_x = axis_x();
	char *anchor = "end";

	for (i = 0; i < num_ticks; i++) {
		if (i != 0) {
			snprintf(line, line_len, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" "
				 "style=\"stroke:lightgray;stroke-width:2;stroke-dasharray:9,12;\"/>\n",
				tick_x, axis_y_off(tick_y),
				axis_x_off(graph_width), axis_y_off(tick_y));
			write_check(plot->fd, line, strlen(line));
		}

		snprintf(line, line_len, "<text x=\"%d\" y=\"%d\" font-family=\"%s\" font-size=\"%d\" "
			 "fill=\"black\" style=\"text-anchor: %s\">%d%s</text>\n",
			text_x,
			axis_y_off(tick_y - tick_font_size / 2),
			font_family, tick_font_size, anchor, first + step * i, units);
		write_check(plot->fd, line, strlen(line));
		tick_y += pixels_per_tick;
	}
	snprintf(line, line_len, "<text x=\"%d\" y=\"%d\" font-family=\"%s\" font-size=\"%d\" "
		 "fill=\"black\" style=\"text-anchor: %s\">%d%s</text>\n",
		 text_x, axis_y_off(graph_height), font_family, tick_font_size, anchor, last, units);
	write_check(plot->fd, line, strlen(line));
}

void set_plot_label(struct plot *plot, char *label)
{
	int len;
	int fd = plot->fd;

	snprintf(line, line_len, "<text x=\"%d\" y=\"%d\" font-family=\"%s\" "
		 "font-size=\"%d\" fill=\"black\" style=\"text-anchor: %s\">%s</text>\n",
		 axis_x() + graph_width / 2,
		 plot_label_height / 2,
		font_family, plot_label_font_size, "middle", label);
	len = strlen(line);
	write_check(fd, line, len);
}

static void close_svg(int fd)
{
	char *close_line = "</svg>\n";

	write_check(fd, close_line, strlen(close_line));
}

int close_plot(struct plot *plot)
{
	close_svg(plot->fd);
	if (plot->direction == PLOT_DOWN)
		plot->start_y_offset += plot->total_height;
	else if (plot->direction == PLOT_ACROSS)
		plot->start_x_offset += plot->total_width;
	return 0;
}

struct plot *alloc_plot(void)
{
	struct plot *plot;
	plot = calloc(1, sizeof(*plot));
	if (!plot) {
		fprintf(stderr, "Unable to allocate memory %s\n", strerror(errno));
		exit(1);
	}
	plot->fd = 0;
	return plot;
}

int close_plot_file(struct plot *plot)
{
	int ret;
	ret = lseek(plot->fd, 0, SEEK_SET);
	if (ret == (off_t)-1) {
		perror("seek");
		exit(1);
	}
	final_width = ((final_width  + 1) / 2) * 2;
	final_height = ((final_height  + 1) / 2) * 2;
	snprintf(line, line_len, "<svg  xmlns=\"http://www.w3.org/2000/svg\" "
		 "width=\"%d\" height=\"%d\">\n",
		 final_width, final_height);
	write_check(plot->fd, line, strlen(line));
	snprintf(line, line_len, "<rect x=\"0\" y=\"0\" width=\"%d\" "
		 "height=\"%d\" fill=\"white\"/>\n", final_width, final_height);
	write_check(plot->fd, line, strlen(line));
	close(plot->fd);
	plot->fd = 0;
	return 0;
}

void set_plot_output(struct plot *plot, char *filename)
{
	int fd;

	if (plot->fd)
		close_plot_file(plot);
	fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY, 0600);
	if (fd < 0) {
		fprintf(stderr, "Unable to open output file %s err %s\n", filename, strerror(errno));
		exit(1);
	}
	plot->fd = fd;
	plot->start_y_offset = plot->start_x_offset = 0;
	write_svg_header(fd);
}

char *byte_unit_names[] = { "", "K", "M", "G", "T", "P", "E", "Z", "Y", "unobtainium" };
int MAX_BYTE_UNIT_SCALE = 9;

char *time_unit_names[] = { "n", "u", "m", "s" };
int MAX_TIME_UNIT_SCALE = 3;

void scale_line_graph_bytes(u64 *max, char **units, u64 factor)
{
	int scale = 0;
	u64 val = *max;
	u64 div = 1;
	while (val > factor * 64) {
		val /= factor;
		scale++;
		div *= factor;
	}
	*units = byte_unit_names[scale];
	if (scale == 0)
		return;

	if (scale > MAX_BYTE_UNIT_SCALE)
		scale = MAX_BYTE_UNIT_SCALE;

	*max /= div;
}

void scale_line_graph_time(u64 *max, char **units)
{
	int scale = 0;
	u64 val = *max;
	u64 div = 1;
	while (val > 1000 * 10) {
		val /= 1000;
		scale++;
		div *= 1000;
		if (scale == MAX_TIME_UNIT_SCALE)
			break;
	}
	*units = time_unit_names[scale];
	if (scale == 0)
		return;

	*max /= div;
}

static int rolling_span(struct graph_line_data *gld)
{
	if (rolling_avg_secs)
		return rolling_avg_secs;
	return (gld->stop_seconds - gld->min_seconds) / 25;
}


double line_graph_roll_avg_max(struct graph_line_data *gld)
{
	unsigned int i;
	int rolling;
	double avg, max = 0;

	rolling = rolling_span(gld);
	for (i = gld->min_seconds; i < gld->stop_seconds; i++) {
		avg = rolling_avg(gld->data, i, rolling);
		if (avg > max)
			max = avg;
	}
	return max;
}

int svg_line_graph(struct plot *plot, struct graph_line_data *gld, char *color, int thresh1, int thresh2)
{
	unsigned int i;
	double val;
	double avg;
	int rolling;
	int fd = plot->fd;
	char *start = "<path d=\"";
	double yscale = ((double)gld->max) / graph_height;
	double xscale = (double)(gld->max_seconds - gld->min_seconds - 1) / graph_width;
	char c = 'M';
	double x;
	int printed_header = 0;
	int printed_lines = 0;

	if (thresh1 && thresh2)
		rolling = 0;
	else
		rolling = rolling_span(gld);

	for (i = gld->min_seconds; i < gld->stop_seconds; i++) {
		avg = rolling_avg(gld->data, i, rolling);
		if (yscale == 0)
			val = 0;
		else
			val = avg / yscale;

		if (val > graph_height)
			val = graph_height;
		if (val < 0)
			val = 0;

		x = (double)(i - gld->min_seconds) / xscale;
		if (!thresh1 && !thresh2) {
			if (!printed_header) {
				write_check(fd, start, strlen(start));
				printed_header = 1;
			}

			/* in full line mode, everything in the graph is connected */
			snprintf(line, line_len, "%c %d %d ", c, axis_x_off(x), axis_y_off(val));
			c = 'L';
			write_check(fd, line, strlen(line));
			printed_lines = 1;
		} else if (avg > thresh1 || avg > thresh2) {
			int len = 10;
			if (!printed_header) {
				write_check(fd, start, strlen(start));
				printed_header = 1;
			}

			/* otherwise, we just print a bar up there to show this one data point */
			if (i >= gld->stop_seconds - 2)
				len = -10;

			/*
			 * we don't use the rolling averages here to show high
			 * points in the data
			 */
			snprintf(line, line_len, "M %d %d h %d ", axis_x_off(x),
				 axis_y_off(val), len);
			write_check(fd, line, strlen(line));
			printed_lines = 1;
		}

	}
	if (printed_lines) {
		snprintf(line, line_len, "\" fill=\"none\" stroke=\"%s\" stroke-width=\"2\"/>\n", color);
		write_check(fd, line, strlen(line));
	}
	if (plot->timeline)
		svg_write_time_line(plot, plot->timeline);

	return 0;
}

void svg_write_time_line(struct plot *plot, int col)
{
	snprintf(line, line_len, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" "
				 "style=\"stroke:black;stroke-width:2;\"/>\n",
				 axis_x_off(col), axis_y_off(0),
				 axis_x_off(col), axis_y_off(graph_height));
	write_check(plot->fd, line, strlen(line));
}

static void svg_add_io(int fd, double row, double col, double width, double height, char *color)
{
	float rx = 0;

	snprintf(line, line_len, "<rect x=\"%.2f\" y=\"%.2f\" width=\"%.1f\" height=\"%.1f\" "
		 "rx=\"%.2f\" style=\"stroke:none;fill:%s;stroke-width:0\"/>\n",
		 axis_x_off_double(col), axis_y_off_double(row), width, height, rx, color);
	write_check(fd, line, strlen(line));
}

int svg_io_graph_movie_array(struct plot *plot, struct pid_plot_history *pph)
{
	double cell_index;
	double movie_row;
	double movie_col;
	int i;

	for (i = 0; i < pph->num_used; i++) {
		cell_index = pph->history[i];
		movie_row = floor(cell_index / graph_width);
		movie_col = cell_index - movie_row * graph_width;
		svg_add_io(plot->fd, movie_row, movie_col, 4, 4, pph->color);
	}
	return 0;
}

static float spindle_steps = 0;

void rewind_spindle_steps(int num)
{
	spindle_steps -= num * 0.01;
}

int svg_io_graph_movie_array_spindle(struct plot *plot, struct pid_plot_history *pph)
{
	double cell_index;
	int i;
	int num_circles = 0;
	double cells_per_circle;
	double circle_num;
	double degrees_per_cell;
	double rot;
	double center_x;
	double center_y;
	double graph_width_extra = graph_width + graph_circle_extra;
	double graph_height_extra = graph_height + graph_circle_extra;
	double radius;;

	if (graph_width_extra > graph_height_extra)
		graph_width_extra = graph_height_extra;

	if (graph_width_extra < graph_height_extra)
		graph_height_extra = graph_width_extra;

	radius = graph_width_extra;

	center_x = axis_x_off_double(graph_width_extra / 2);
	center_y = axis_y_off_double(graph_height_extra / 2);

	snprintf(line, line_len, "<g transform=\"rotate(%.4f, %.2f, %.2f)\"> "
		 "<circle cx=\"%.2f\" cy=\"%.2f\" "
		 "stroke=\"black\" stroke-width=\"6\" "
		 "r=\"%.2f\" fill=\"none\"/>\n",
		 spindle_steps * 1.2, center_x, center_y, center_x, center_y, graph_width_extra / 2);
	write_check(plot->fd, line, strlen(line));
	snprintf(line, line_len, "<circle cx=\"%.2f\" cy=\"%.2f\" "
		"stroke=\"none\" fill=\"red\" r=\"%.2f\"/>\n</g>\n",
		axis_x_off_double(graph_width_extra), center_y, 4.5);
	write_check(plot->fd, line, strlen(line));
	spindle_steps += 0.01;

	radius = floor(radius / 2);
	num_circles = radius / 4 - 3;
	cells_per_circle = pph->history_max / num_circles;
	degrees_per_cell = 360 / cells_per_circle;

	for (i = 0; i < pph->num_used; i++) {
		cell_index = pph->history[i];
		circle_num = floor(cell_index / cells_per_circle);
		rot = cell_index - circle_num * cells_per_circle;
		circle_num = num_circles - circle_num;
		radius = circle_num * 4;

		rot = rot * degrees_per_cell;
		rot -= spindle_steps;
		snprintf(line, line_len, "<path transform=\"rotate(%.4f, %.2f, %.2f)\" "
			 "d=\"M %.2f %.2f a %.2f %.2f 0 0 1 0 5\" "
			 "stroke=\"%s\" stroke-width=\"4\"/>\n",
			 -rot, center_x, center_y,
			 axis_x_off_double(graph_width_extra / 2 + radius) + 8, center_y,
			 radius, radius, pph->color);

		write_check(plot->fd, line, strlen(line));
	}
	return 0;
}

static int add_plot_history(struct pid_plot_history *pph, double val)
{
	if (pph->num_used == pph->history_len) {
		pph->history_len += 4096;
		pph->history = realloc(pph->history,
				       pph->history_len * sizeof(double));
		if (!pph->history) {
			perror("Unable to allocate memory");
			exit(1);
		}
	}
	pph->history[pph->num_used++] = val;
	return 0;
}

int svg_io_graph_movie(struct graph_dot_data *gdd, struct pid_plot_history *pph, int col)
{
	int row = 0;
	int arr_index;
	unsigned char val;
	int bit_index;
	int bit_mod;
	double blocks_per_row = (gdd->max_offset - gdd->min_offset + 1) / gdd->rows;
	double movie_blocks_per_cell = (gdd->max_offset - gdd->min_offset + 1) / (graph_width * graph_height);
	double cell_index;
	int margin_orig = graph_inner_y_margin;

	graph_inner_y_margin += 5;
	pph->history_max = (gdd->max_offset - gdd->min_offset + 1) / movie_blocks_per_cell;

	for (row = gdd->rows - 1; row >= 0; row--) {
		bit_index = row * gdd->cols + col;
		arr_index = bit_index / 8;
		bit_mod = bit_index % 8;

		if (arr_index < 0)
			continue;
		val = gdd->data[arr_index];
		if (val & (1 << bit_mod)) {
			/* in bytes, linear offset from the start of the drive */
			cell_index = (double)row * blocks_per_row;

			/* a cell number in the graph */
			cell_index /= movie_blocks_per_cell;

			add_plot_history(pph, cell_index);
		}
	}
	graph_inner_y_margin = margin_orig;
	return 0;
}

int svg_io_graph(struct plot *plot, struct graph_dot_data *gdd)
{
	int fd = plot->fd;;
	int col = 0;
	int row = 0;
	int arr_index;
	unsigned char val;
	int bit_index;
	int bit_mod;

	for (row = gdd->rows - 1; row >= 0; row--) {
		for (col = 0; col < gdd->cols; col++) {
			bit_index = row * gdd->cols + col;
			arr_index = bit_index / 8;
			bit_mod = bit_index % 8;

			if (arr_index < 0)
				continue;
			val = gdd->data[arr_index];
			if (val & (1 << bit_mod))
				svg_add_io(fd, floor(row / io_graph_scale), col, 1.5, 1.5, gdd->color);
		}
	}
	return 0;
}

void svg_alloc_legend(struct plot *plot, int num_lines)
{
	char **lines = calloc(num_lines, sizeof(char *));
	plot->legend_index = 0;
	plot->legend_lines = lines;
	plot->num_legend_lines = num_lines;
}

void svg_free_legend(struct plot *plot)
{
	int i;
	for (i = 0; i < plot->legend_index; i++)
		free(plot->legend_lines[i]);
	free(plot->legend_lines);
	plot->legend_lines = NULL;
	plot->legend_index = 0;
}

void svg_write_legend(struct plot *plot)
{
	int legend_line_x = axis_x_off(graph_width) + legend_x_off;
	int legend_line_y = axis_y_off(graph_height) + legend_y_off;
	int i;

	if (plot->legend_index == 0)
		return;

	snprintf(line, line_len, "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" "
		 "fill=\"white\" filter=\"url(#shadow)\"/>\n",
		 legend_line_x - 15,
		 legend_line_y - 12,
		 legend_width,
		 plot->legend_index * legend_font_size + legend_font_size / 2 + 12);

	write_check(plot->fd, line, strlen(line));
	for (i = 0; i < plot->legend_index; i++) {
		write_check(plot->fd, plot->legend_lines[i],
		      strlen(plot->legend_lines[i]));
		free(plot->legend_lines[i]);
	}
	free(plot->legend_lines);
	plot->legend_lines = NULL;
	plot->legend_index = 0;
}

void svg_add_legend(struct plot *plot, char *text, char *extra, char *color)
{
	int legend_line_x = axis_x_off(graph_width) + legend_x_off;
	int legend_line_y = axis_y_off(graph_height) + legend_y_off;

	if (!text && (!extra || strlen(extra) == 0))
		return;

	legend_line_y += plot->legend_index * legend_font_size + legend_font_size / 2;
	snprintf(line, line_len, "<path d=\"M %d %d h 8\" stroke=\"%s\" stroke-width=\"8\" "
		 "filter=\"url(#labelshadow)\"/> "
		 "<text x=\"%d\" y=\"%d\" font-family=\"%s\" font-size=\"%d\" "
		 "fill=\"black\" style=\"text-anchor: left\">%s%s</text>\n",
		 legend_line_x, legend_line_y,
		 color, legend_line_x + 13,
		 legend_line_y + 4, font_family, legend_font_size,
		 text, extra);

	plot->legend_lines[plot->legend_index++] = strdup(line);
}

void set_legend_width(int longest_str)
{
	if (longest_str)
		legend_width = longest_str * (legend_font_size * 3 / 4) + 25;
	else
		legend_width = 0;
}

void set_rolling_avg(int rolling)
{
	rolling_avg_secs = rolling;
}

void set_io_graph_scale(int scale)
{
	io_graph_scale = scale;
}

void set_graph_size(int width, int height)
{
	graph_width = width;
	graph_height = height;
}

void get_graph_size(int *width, int *height)
{
	*width = graph_width;
	*height = graph_height;
}

void set_graph_height(int h)
{
	graph_height = h;
}
void set_graph_width(int w)
{
	graph_width = w;
}
