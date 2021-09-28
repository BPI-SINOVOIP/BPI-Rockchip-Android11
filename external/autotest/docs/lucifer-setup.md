# Setting up Lucifer

Install [depot_tools].

[depot_tools]: https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up

Get the Lucifer binaries:

    binary_dir=~/lucifer
    mkdir -p "$binary_dir"
    cipd install -root "$binary_dir" chromiumos/infra/lucifer prod

Add the path to `shadow_config.ini`:

    cat <<EOF >>/usr/local/autotest/shadow_config.ini
    [LUCIFER]
    binaries_path: $binary_dir/usr/bin
    EOF
