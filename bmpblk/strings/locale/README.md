# Firmware Localized Texts

This directory contains translations for the message strings shown in the
firmware UI.

## Steps to Modify/Add Messages
The message translation process works as follows:

1. If you make a code change that requires message changes, adjust the English
strings in the `firmware_strings.grd` file. During the development period,
limits the locale to en to skip the missing translations.

```
(chroot) LOCALES=en BOARD="$BOARD" make
# Or
(chroot) LOCALES=en emerge-$BOARD chromeos-bmpblk
```

2. To trigger translation of updated messages, take the the current version of
`firmware_strings.grd` and update the [copy in Google's internal code repository
](https://cs.corp.google.com/piper///depot/google3/googleclient/chrome/transconsole_resources/strings/cros/firmware_strings.grd)

```shell
$ prodaccess
$ g4d -f localization   # Create (force) a new client and cd to google3 directory
$ g4 sync               # Sync to the latest changelist
$ cp $CROS/src/platform/bmpblk/strings/locale/firmware_strings.grd googleclient/chrome/transconsole_resources/strings/cros/firmware_strings.grd
$ g4 diff               # Display the differences between files
$ g4 change             # Create a changelist (from default changelist)
```

3. Wait until the actual translation process finishes (may take a week).

4. Build `all_xtbs` target in the Google's internal code repository, and copy
the resulting `firmware_strings_${LOCALE}.xtb` files which contain the
translated strings into your Chromium OS local branch.

```shell
$ prodaccess
$ g4d localization      # cd to google3 directory
$ g4 sync               # Sync to the latest changelist
$ blaze build //googleclient/chrome/transconsole_resources:all_xtbs
# Get the diff of message ID of en-GB. This can help you find the message ID
# related to the change.
$ python3 $CROS/src/platform/bmpblk/update_xtb.py diff en-GB
# Merge the strings with those message ID.
$ python3 $CROS/src/platform/bmpblk/update_xtb.py merge <ID1> <ID2> ...
```

5. Create a code review that updates the corresponding files in this directory.
Do a readiness check to verify that your review only contains string changes
(i.e. no unexpected message ID changes).

6. Have your code change reviewed and submit it as usual.
