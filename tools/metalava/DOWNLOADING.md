# Checking out Metalava

Metalava can be downloaded from the `metalava-master` manifest branch via `repo` as explained below

## To check out `metalava-master` using `repo`:
1. Install `repo` (Repo is a tool that makes it easier to download multiple Git repositories at once. For more information about Repo, see the [Repo Command Reference](https://source.android.com/setup/develop/repo))

```bash
mkdir ~/bin
PATH=~/bin:$PATH
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod a+x ~/bin/repo
```

2. Configure Git with your real name and email address.

```bash
git config --global user.name "Your Name"
git config --global user.email "you@example.com"
```

3. Create a directory for your checkout (it can be any name)

```bash
mkdir metalava-master
cd metalava-master
```

4. Use `repo` command to initialize the repository.

```bash
repo init -u https://android.googlesource.com/platform/manifest -b metalava-master
```

5. Now your repository is configured to pull only what you need for building and running Metalava. Download the code (this may take some time; the checkout is about 1.7G):

```bash
repo sync -j8 -c
```
## Checking out `aosp/master` instead:

For anyone that is already working in the `aosp/master` branch, you can use that repo checkout instead. For small changes to metalava, this is not recommended - it is a very large checkout, with many dependencies not used by metalava.

## Developing

See also [README.md](README.md) for details about building and running Metalava after you have checked out the code.

