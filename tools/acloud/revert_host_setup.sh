#!/bin/bash

function remove_cuttlefish_pkgs() {
    local PACKAGES=("cuttlefish-common"
                    "ssvnc"
                    "qemu-kvm"
                    "qemu-system-common"
                    "qemu-system-x86"
                    "qemu-utils"
                    "libvirt-clients"
                    "libvirt-daemon-system")
    for package in ${PACKAGES[@]};
    do
        echo " - uninstalling $package"
        sudo su -c "apt-get purge $package -y && apt-get autoremove -y"
    done
}

function remove_cuttlefish_usergroups() {
    local GROUPS_TO_REMOVE=("kvm" "libvirt" "cvdnetwork")
    echo " - remove user from groups: ${GROUPS_TO_REMOVE[@]}"
    for g in ${GROUPS_TO_REMOVE[@]};
    do
        sudo gpasswd -d $USER $g
    done
}

function remove_configs() {
    local ACLOUD_CONFIG_DIR=~/.config/acloud
    if [ -d $ACLOUD_CONFIG_DIR ]; then
        echo " - remove acloud configs"
        rm -rf $ACLOUD_CONFIG_DIR
    fi

    local ACLOUD_SSH_KEY=~/.ssh/acloud_rsa
    if [ -f $ACLOUD_SSH_KEY ]; then
        echo " - remove acloud ssh keys"
        rm ${ACLOUD_SSH_KEY}*
    fi

    local ACLOUD_VNC_PROFILE=~/.vnc/profiles/acloud_vnc_profile.vnc
    if [ -f $ACLOUD_VNC_PROFILE ]; then
        echo " - remove acloud vnc profile"
        rm $ACLOUD_VNC_PROFILE
    fi
}

function purge_cuttlefish_host_setup(){
    echo "Purging host of acloud setup steps..."
    remove_cuttlefish_pkgs
    remove_cuttlefish_usergroups
    remove_configs
    echo "Done!"
}

purge_cuttlefish_host_setup
