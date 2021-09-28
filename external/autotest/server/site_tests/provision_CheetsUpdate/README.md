# provision_CheetsUpdate

## Running the autotest locally

In order to test your changes, run the autotest locally with:

```shell
(chroot) $ test_that IP provision_CheetsUpdate --args='value=git_nyc-mr1-arc/cheets_arm-user/4885137' -b kevin
```

## Running push\_to\_device manually

Go to [Android Build Status Dashboard](http://go/ab), select the target build,
download push\_to\_device.zip and the prebuilt image (e.g. cheets_arm-img-4885137.zip)
from the Artifacts tab.

![Download push_to_device.zip from go/ab](https://screenshot.googleplex.com/GBajT9u1bis.png)

```shell
$ unzip push_to_device.zip -d ./ptd
$ python3 ./ptd/push_to_device.py --use-prebuilt-file ./cheets_arm-img-4885137.zip \
--simg2img /usr/bin/simg2img --mksquashfs-path ./ptd/bin/mksquashfs \
--unsquashfs-path ./ptd/bin/unsquashfs --shift-uid-py-path ./ptd/shift_uid.py IP --loglevel DEBUG
```

Warning: If the command fails, check run\_push\_to\_device method in
[provision_CheetsUpdate.py](https://chromium.googlesource.com/chromiumos/third_party/autotest/+/master/server/site_tests/provision_CheetsUpdate/provision_CheetsUpdate.py) for required params.

## Updating the autotest

After you submit your changes, [Chrome OS lab deputy will push it to
prod](https://sites.google.com/a/google.com/chromeos/for-team-members/infrastructure/chromeos-admin/push-to-prod).
Send a heads up to [Chrome OS lab
deputy](https://sites.google.com/a/google.com/chromeos/for-team-members/infrastructure/chrome-os-infrastructure-deputy)
that this change is there.

Make sure that it is possible to pass
[presubmit](https://atp.googleplex.com/tests/arc++/presubmit?state=COMPLETED&testLabelName=PRESUBMIT&tabId=test_run)
after the change is pushed to prod.
