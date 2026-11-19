// kanata-restart-on-attach: restarts a launchd service when a keyboard is
// attached.
//
// kanata does not grab a keyboard that connects after it has started, so a
// keyboard that is replugged (or re-enumerates after sleep) types unmapped
// until kanata is restarted. A launchd job with a com.apple.iokit.matching
// LaunchEvents entry is started when a matching device appears; this is the
// program that job runs. See README.md for the job definition.
//
// It exists because launchd keeps relaunching such a job until the pending
// events are consumed through the XPC event stream, which a shell script
// cannot do. A keyboard publishes several matching IOKit nodes at once, so
// events arriving within the settle window are coalesced into one restart;
// the window also gives the device time to finish enumerating.
//
// The only thing it runs is `/bin/launchctl kickstart -k <service-target>`.
// It lives in a signed bundle that holds privacy grants, so it does not run
// commands of the caller's choosing.
//
// Usage: kanata-restart-on-attach <service-target>
//   e.g. kanata-restart-on-attach gui/501/com.kanata

#include <dispatch/dispatch.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <xpc/xpc.h>

#define SETTLE_SECONDS 2

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <service-target>\n", argv[0]);
        return 64;
    }
    const char *target = argv[1];

    xpc_set_event_stream_handler(
        "com.apple.iokit.matching", dispatch_get_main_queue(),
        ^(xpc_object_t event) {
            static bool scheduled;
            if (scheduled) return;
            scheduled = true;
            dispatch_after(
                dispatch_time(DISPATCH_TIME_NOW, SETTLE_SECONDS * NSEC_PER_SEC),
                dispatch_get_main_queue(), ^{
                    execl("/bin/launchctl", "launchctl", "kickstart", "-k",
                          target, (char *)NULL);
                    perror("/bin/launchctl");
                    _exit(1);
                });
        });

    dispatch_main();
}
