#ifndef STUPID__SIGNAL_HANDLER_HPP
#define STUPID__SIGNAL_HANDLER_HPP

#include "common/signal.hpp"

namespace stupid {
namespace common {

void sighup_handler(int signum);
void install_sighandler(int signum, signal_handler_t handler, int flags);
void install_fatal_sighandlers(void);

} //namespace common
} //namespace stupid

#endif //STUPID__SIGNAL_HANDLER_HPP
