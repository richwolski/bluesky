#ifndef _UTIL_H_ 
#define _UTIL_H_ 1

#include <boost/version.hpp>

#if BOOST_VERSION <= 103500
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/bind.hpp>
#include <asio.hpp>
#include <asio/buffer.hpp>
//typedef boost::condition condition_variable ;
//typedef boost::detail::thread::scoped_lock<boost::mutex> scoped_lock;
using asio::ip::tcp;
using asio::error_code;
using asio::buffers_begin;
namespace syserr=asio;
#else
#if BOOST_VERSION < 104000
#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/bind.hpp>
using boost::asio::ip::tcp;
using boost::system::error_code;
using boost::system::system_error;
namespace syserr=boost::system;
#else
#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>
using boost::asio::ip::tcp;
using boost::system::error_code;
using boost::system::system_error;
namespace syserr=boost::system;
#endif
#endif

#endif
