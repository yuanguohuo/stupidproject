#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <iostream>
#include <atomic>

#include "common/signal_handler.hpp"
#include "common/thread.hpp"
#include "common/backtrace.hpp"
#include "common/dout.hpp"
#include "common/thread.hpp"

//NOTICE:
//  1. 当一个signal被处理的时候(正在handler函数里)，这个signal会被系统自动mask；处理完成之后，会被系统自动
//     unmask；
//  2. 当一个signal被raise的时候，它恰好被mask了(无论是被系统自动mask的————正在处理上次触发，还是程序手动
//     mask的)，它都将会处于pending状态，等到这个singal被unmask(无论是上次触发处理完成进而被系统自动unmask
//     的，还是程序手动unmask的)之后再处理。但是，raise多次只会处理一次(handler被调用一次)；也就是说，同一个
//     signal，系统只维护它的一个pending；
//  3. 在一个singal的handler函数中，可以再raise它自己。因为这时它被自动mask了(见上面第1条)，所以不会立即处理，
//     而是处于pending状态(见上面第2条)，直到当前handler执行完成进而singal被自动unmask后再处理，即再次调用
//     handler；handler再次raise它自己，以此无限循环，进程不会crash。注意：这种情况下handler没有被递归调用，
//     而是一次执行完成之后，再执行下一次。
//     若是在raise它自己之前，手动unmask这个signal(在handler中也可以手动unmask)，那么raise时会立即处理，立即
//     调用handler(当前调用还没退出)，故导致无限递归，直到栈溢出，进程crash。
//  4. 在singal-A的handler中不单可以再次raise自己，也可以raise其它signal，例如signal-B；如果signal-B此时没有
//     被mask，那么它会被立即处理(即不等singal-A的handler退出)。所以，这也是一种handler重入。
//     当然，若signal-B此时被mask了，那它也将处于pending状态，要等到unmask后才处理。
//  5. 在signal handler中，还可以把自己的handler改成别的函数(例如改回SIG_DFL)，这样下一次raise，将调用新的函数
//     (SIG_DFL)。
//
//Yuanguo：在linux和macOS上测试，都是如上结果;

namespace stupid {
namespace common {

void sighup_handler(int signum)
{
  //TODO:
  //reopen_logs();
  char thread_name[32];
  memset(thread_name, '\0', 32);
  pthread_getname_wrapper(::pthread_self(), thread_name, 32);
  pid_t tid = gettid_wrapper();
  std::cerr << __func__ << " signum: " << signum << " thread: " << tid << " thread-name: " << thread_name << std::endl;
}

static void reraise_fatal(int signum)
{
  // Use default handler to dump core
  signal(signum, SIG_DFL);
  int ret = raise(signum);

  // Normally, we won't get here. If we do, something is very weird.
  char buf[1024];
  if (ret) {
    snprintf(buf, sizeof(buf), "reraise_fatal: failed to re-raise signal %d\n", signum);
    dout_emergency(buf);
  } else {
    snprintf(buf, sizeof(buf), "reraise_fatal: default handler for signal %d didn't terminate the process?\n", signum);
    dout_emergency(buf);
  }
  exit(1);
}

static void handle_oneshot_fatal_signal(int signum)
{
  constexpr static pid_t NULL_TID{0};

  //Yuanguo: 注意handler_tid是static的，也就是说它全局只有一份。多个信号，或者一个信号的多次处理
  //  都公用这个变量。
  static std::atomic<pid_t> handler_tid{NULL_TID};

  //Yuanguo: 确保在同一时间只有一个线程能够处理fatal信号，避免多个线程同时进入信号处理程序
  //
  //- 如果handler_tid == expected(NULL_TID):
  //    - 原子地执行handler_tid = gettid_wrapper()，并返回true; 也就是说，第一个执行
  //      handle_oneshot_fatal_signal()的线程会把自己的线程ID(记为threadA)存到handler_id;
  //    - 这说明没有其它线程触发过任何fatal信号；if条件不满足(!true)，跳过if{}，处理信号；
  //- 如果handler_tid != expected(NULL_TID):
  //    - expected = handler_tid.load()，并返回false；
  //    - 这说明fatal信号已经被触发，正在被threadA处理。
  //    - 注意此时handler_tid的值是threadA，所以执行expected = threadA，然后进入if-body;
  if (auto expected{NULL_TID}; !handler_tid.compare_exchange_strong(expected, gettid_wrapper())) {
    if (expected == gettid_wrapper()) {
      //Yuanguo: 正在处理fatal信号的线程就是自己(threadA)；是这样发生的：
      //  - 第一个fatal信号，例如SIGABRT，被raise；被分给threadA处理，第一次进入handle_oneshot_fatal_signal(signum=SIGABRT)；
      //  - 在处理的过程中，又触发了一个fatal信号，例如SIGSEGV(不会是SIGABRT，因为它被自动mask了，不会立即处理，也就不会重入
      //    handle_oneshot_fatal_signal函数)，又被分给threadA处理。
      //    这里就是处理SIGSEGV的逻辑，即第二次进入handle_oneshot_fatal_signal(signum=SIGSEGV)。
      //    处理方式是：signal(SIGSEGV, SIG_DFL); 即把SIGSEGV的handler换成默认，然后返回。
      //    其实这次SIGSEGV没有处理，只是替换它的handler，这有什么作用呢？继续往下看！
      //  - 第二次handle_oneshot_fatal_signal(signum=SIGSEGV)返回，第一次handle_oneshot_fatal_signal(signum=SIGABRT)继续执行：
      //    若它再触发SIGSEGV，就会使用SIG_DFL来处理(presumably dump core)；

      // The handler code may itself trigger a SIGSEGV if the heap is corrupt.
      // In that case, SIG_DFL followed by return specifies that the default
      // signal handler -- presumably dump core -- will handle it.
      signal(signum, SIG_DFL);
    } else {
      //Yuanguo: 正在处理fatal信号的是threadA，而自己是threadB(threadB!=threadA)。是这样发生的：
      //  - 第一个fatal信号，例如SIGABRT，被raise；被分给threadA处理，第一次进入handle_oneshot_fatal_signal(signum=SIGABRT)；
      //  - 此时又有一个fatal信号(例如SIGSEGV)被raise：
      //      - threadA在处理的过程中，即在handle_oneshot_fatal_signal(signum=SIGABRT)中，又触发了SIGSEGV；
      //      - 或者，进程的其它地方又触发了SIGSEGV；
      //  - SIGSEGV被分给threadB处理。这里就是threadB处理SIGSEGV的逻辑，即第二次进入handle_oneshot_fatal_signal(signum=SIGSEGV)。
      //    这里threadB根本不处理；因为多个线程都遇到致命错误，只要第一个线程真正处理就行，其余的线程不处理，以避免重复处理或潜
      //    在的死循环

      // Huh, another thread got into troubles while we are handling the fault.
      // If this is i.e. SIGSEGV handler, returning means retrying the faulty
      // instruction one more time, and thus all those extra threads will run
      // into a busy-wait basically.
    }
    return;
  }

  //Yuanguo: 下面是第一次(也是唯一一次)处理fatal信号.

  char buf[1024];
  char pthread_name[16] = {0}; //limited by 16B include terminating null byte.
  int r = pthread_getname_wrapper(::pthread_self(), pthread_name, sizeof(pthread_name));
  (void)r;

  snprintf(buf, sizeof(buf), "*** Caught signal (%s) in thread:%llx thread_name:%s ***\n",
      sig_str(signum),
      (unsigned long long)pthread_self(),
	    pthread_name);

  dout_emergency(buf);

  //TODO:
  //pidfile_remove();

  ClibBackTrace bt;
  //we don't allocate memory here;
  //we are not using ostringstream here; it could call malloc(), which we
  // don't want inside a signal handler.
  //TODO: Also fix the backtrace code not to allocate memory.
  char backtrace[1024];
  bt.print(backtrace, 1024);

  dout_emergency(backtrace);

  //TODO:
  bool is_inside_log_lock = false;
  // avoid recursion back into logging code if that is where
  // we got the SEGV.
  if (!is_inside_log_lock) {
    // dump to log.  this uses the heap extensively, but we're better
    // off trying than not.
  }

  //TODO:
  bool g_eio = false;
  if (g_eio) {
    // if this was an EIO crash, we don't need to trigger a core dump,
    // since the problem is hardware, or some layer beneath us.
    _exit(EIO);
  } else {
    reraise_fatal(signum);
  }
}

void install_sighandler(int signum, signal_handler_t handler, int flags)
{
  int ret;
  struct sigaction oldact;
  struct sigaction act;
  memset(&act, 0, sizeof(act));

  act.sa_handler = handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = flags;

  ret = sigaction(signum, &act, &oldact);
  if (ret != 0) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "install_sighandler: sigaction returned "
        "%d when trying to install a signal handler for %s\n",
        ret, sig_str(signum));
    dout_emergency(buf);
    exit(1);
  }
}

void install_fatal_sighandlers(void)
{
  install_sighandler(SIGSEGV, handle_oneshot_fatal_signal, SA_NODEFER);
  install_sighandler(SIGABRT, handle_oneshot_fatal_signal, SA_NODEFER);
  install_sighandler(SIGBUS,  handle_oneshot_fatal_signal, SA_NODEFER);
  install_sighandler(SIGILL,  handle_oneshot_fatal_signal, SA_NODEFER);
  install_sighandler(SIGFPE,  handle_oneshot_fatal_signal, SA_NODEFER);
  install_sighandler(SIGXCPU, handle_oneshot_fatal_signal, SA_NODEFER);
  install_sighandler(SIGXFSZ, handle_oneshot_fatal_signal, SA_NODEFER);
  install_sighandler(SIGSYS,  handle_oneshot_fatal_signal, SA_NODEFER);
}

} //namespace common
} //namespace stupid
