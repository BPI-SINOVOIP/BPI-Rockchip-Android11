# **Acloud Manual**

## **Acloud**

Acloud is a command line tool that assists users to create an Android Virtual
Device (AVD).

## **Table of Contents**

1. [Environment Setup](#Environment-Setup)
2. [Basic Usage](#Basic-Usage)

### **Environment Setup**

Add acloud to your $PATH by lunching your environment.

* Source envsetup.sh from the root of the android source checkout

```
$ source build/envsetup.sh
```

* Run lunch for an AVD target

```
$ lunch aosp_cf_x86_phone-userdebug
```


You should now be able to call acloud from anywhere.

### **Basic Usage**

Acloud commands take the following form:

**acloud &lt;command&gt; &lt;optional arguments&gt;**

Available commands:

* [create](#create)
* [list](#list)
* [delete](#delete)
* [reconnect](#reconnect)
* [setup](#setup)
* [pull](#pull)

#### **create**

Create is the main entry point in creating an AVD, supporting both remote
instance (running on a virtual machine in the cloud) and local instance
(running on your local host) use cases. You also have the option to use
a locally built image or an image from the Android Build servers.

**Disclaimer: Creation of a cuttlefish local instance is not formally supported, please use at your own risk.**

Here's a quick cheat-sheet for the 4 use cases:

* Remote instance using an Android Build image (LKGB (Last Known Good Build)
for cuttlefish phone target in the branch of your repo, default aosp master
if we can't determine it)

> $ acloud create

* Remote instance using a locally built image (use `m` to build the image)

> $ acloud create --local-image [optional local-image-path]

* Local instance using an Android Build image (LKGB for cuttlefish phone
target in the branch of your repo)

> $ acloud create --local-instance

* Local instance using a locally built image (use `m` to build the image)

> $ acloud create --local-instance --local-image

When specifying an Android Build image, you can specify the branch,
target and/or build id (e.g. `--branch my_branch`). Acloud will assume the
following if they're not specified:

* `--branch`: The branch of the repo you're running the acloud command in, e.g.
in an aosp repo on the master branch, acloud will infer the aosp-master branch.

* `--build-target`: Defaults to the phone target for cuttlefish (e.g.
aosp\_cf\_x86\_phone-userdebug in aosp-master).

* `--build-id`: Default to the Last Known Good Build (LKGB) id for the branch and
target set from above.

Additional helpful create options are:

* `--flavor`: This can be one of phone, auto, wear, tv, iot, tablet and 3g.
This will be used to choose the default hw properties and infer build target
if not specified.

* `--autoconnect`: This defaults to true and upon creation of a remote instance,
creates a ssh tunnel to enable adb and vnc connection to the instance. For the
local instance, acloud will just invoke the vnc client. If you don't want
autoconnect, you can pass in `--no-autoconnect`

* `--unlock`: This can unlock screen after invoke the vnc client.

* `--hw-property`: This is a string where you can specify the different
properties of the AVD. You can specify the cpu, resolution, dpi, memory,and/or
disk in a key:value format like so:
`cpu:2,resolution:1280x700,dpi:160,memory:2g,disk:2g`

* `--reuse-gce`: 'cuttlefish only' This can help users use their own instance.
Reusing specific gce instance if `--reuse-gce` [instance-name] is provided.
Select one gce instance to reuse if `--reuse-gce` is provided.

The full list of options are available via `--help`

> $ acloud create --help

#### **list**

List will retrieve all the remote instances you've created in addition to
any local instances created as well.

Cheatsheet:

* List will show device IP address, adb port and instance name.

> $ acloud list

* List -v will show more detail info on the list.

> $ acloud list -v


#### **delete**

Delete will stop your remote and local instances. Acloud will find all
instances created by you and stop them. If more than one instance is found
(remote or local), you will be prompted to select which instance you would
like to stop.

Cheatsheet:

* Delete sole instance or prompt user with list of instances to delete.

> $ acloud delete

* Delete all instances

> $ acloud delete --all

* Delete a specific instance

> $ acloud delete --instance-names [instance-name]

#### **reconnect**

Reconnect will re-establish ssh tunnels for adb/vnc port forwarding for all
remote instance created by you. It will then look for any devices missing in
`adb devices` and reconnect them to adb. Lastly it will restart vnc for all
devices that don't already have vnc started for them.

Cheatsheet:

* Reconnect sole instance or prompt user with list of instances to reconnect.

> $ acloud reconnect

* Reconnect all instances

> $ acloud reconnect --all

* Reconnect a specific instance

> $ acloud reconnect --instance-names [instance-name]


### **pull**

Pull will provide all log files to download or show in screen. It is helpful
to debug about AVD boot up fail or AVD has abnromal behaviors.

Cheatsheet:

* Pull logs from a sole instance or prompt user to choose one to pull if where
are more than one active instances.

> $ acloud pull

* Pull logs from the specific instance.

> $ acloud pull --instance-name "instance-name"

* Pull a specific log file from a specific instance

> $ acloud pull --instance-name "instance-name" --file-name "file-name"


#### **setup**

Setup will walk you through the steps needed to set up your local host to
create a remote or local instance. It will automatically get invoked through
the create command if we detect some necessary setup steps are required. This
is a standalone command if the user wants to do setup separate from the create
command.

Cheatsheet:

* Run setup for remote/local instances

> $ acloud setup

* Run setup for remote instances only

> $ acloud setup --gcp-init

* Run setup for local instances only

> $ acloud setup --host

* Force run setup

> $ acloud setup --force

* * *

If you have any questions or feedback, contact [acloud@google.com](mailto:acloud@google.com).

If you have any bugs or feature requests, [go/acloud-bug](http://go/acloud-bug).
