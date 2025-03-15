#ifndef STUPID__SIGNAL_HANDLER_ASYNC_HPP
#define STUPID__SIGNAL_HANDLER_ASYNC_HPP

#include "common/signal.hpp"

namespace stupid {
namespace common {

void start_async_signal_handler();
void shutdown_async_signal_handler();

void register_async_signal_handler(int signum, signal_handler_t handler);
void register_async_signal_handler_oneshot(int signum, signal_handler_t handler);
void unregister_async_signal_handler(int signum, signal_handler_t handler);

void queue_async_signal(int signum);

} //namespace common
} //namespace stupid

#endif //STUPID__SIGNAL_HANDLER_ASYNC_HPP
