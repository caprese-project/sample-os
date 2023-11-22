#include <errno.h>
#include <signal.h>
#include <stdlib.h>

static void (*__signal_handlers[NSIG])(int);

int raise(int sig) {
  if (sig < 0 || sig >= NSIG) {
    errno = EINVAL;
    return -1;
  }

  void (*handler)(int) = __signal_handlers[sig];
  if (handler == SIG_DFL) {
    _Exit(1);
  } else if (handler == SIG_IGN) {
    // Do nothing.
  } else if (handler == SIG_ERR) {
    errno = EINVAL;
    return -1;
  } else {
    __signal_handlers[sig] = SIG_DFL;
    handler(sig);
  }

  return 0;
}

void (*signal(int sig, void (*handler)(int)))(int) {
  if (sig < 0 || sig >= NSIG) {
    errno = EINVAL;
    return SIG_ERR;
  }

  void (*old_handler)(int) = __signal_handlers[sig];
  __signal_handlers[sig]   = handler;
  return old_handler;
}
