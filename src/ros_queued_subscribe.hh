//
// Copyright (c) 2017-2018 CNRS
// Authors: Joseph Mirabel
//
//
// This file is part of dynamic_graph_bridge
// dynamic_graph_bridge is free software: you can redistribute it
// and/or modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation, either version
// 3 of the License, or (at your option) any later version.
//
// dynamic_graph_bridge is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Lesser Public License for more details.  You should have
// received a copy of the GNU Lesser General Public License along with
// dynamic_graph_bridge  If not, see
// <http://www.gnu.org/licenses/>.

#ifndef DYNAMIC_GRAPH_ROS_QUEUED_SUBSCRIBE_HH
# define DYNAMIC_GRAPH_ROS_QUEUED_SUBSCRIBE_HH
# include <map>

# include <boost/shared_ptr.hpp>
# include <boost/thread/mutex.hpp>

# include <dynamic-graph/entity.h>
# include <dynamic-graph/signal-time-dependent.h>
# include <dynamic-graph/signal-ptr.h>
# include <dynamic-graph/command.h>
# include <sot/core/matrix-geometry.hh>

# include <ros/ros.h>

# include "converter.hh"
# include "sot_to_ros.hh"

namespace dynamicgraph
{
  class RosQueuedSubscribe;

  namespace command
  {
    namespace rosQueuedSubscribe
    {
      using ::dynamicgraph::command::Command;
      using ::dynamicgraph::command::Value;

# define ROS_QUEUED_SUBSCRIBE_MAKE_COMMAND(CMD)			\
      class CMD : public Command			\
      {							\
      public:						\
	CMD (RosQueuedSubscribe& entity,				\
	     const std::string& docstring);		\
	virtual Value doExecute ();			\
      }

      ROS_QUEUED_SUBSCRIBE_MAKE_COMMAND(Add);
      ROS_QUEUED_SUBSCRIBE_MAKE_COMMAND(Clear);
      ROS_QUEUED_SUBSCRIBE_MAKE_COMMAND(List);
      ROS_QUEUED_SUBSCRIBE_MAKE_COMMAND(Rm);
      ROS_QUEUED_SUBSCRIBE_MAKE_COMMAND(ClearQueue);
      ROS_QUEUED_SUBSCRIBE_MAKE_COMMAND(QueueSize);
      ROS_QUEUED_SUBSCRIBE_MAKE_COMMAND(ReadQueue);

#undef ROS_QUEUED_SUBSCRIBE_MAKE_COMMAND

    } // end of namespace rosQueuedSubscribe.
  } // end of namespace command.

  class RosQueuedSubscribe;

  namespace internal
  {
    template <typename T>
    struct Add;

    struct BindedSignalBase {
      typedef boost::shared_ptr<ros::Subscriber> Subscriber_t;

      BindedSignalBase(RosQueuedSubscribe* e) : entity(e) {}
      virtual ~BindedSignalBase() {}

      virtual void clear () = 0;
      virtual std::size_t size () const = 0;

      Subscriber_t subscriber;
      RosQueuedSubscribe* entity;
    };

    template <typename T, int BufferSize>
    struct BindedSignal : BindedSignalBase {
      typedef dynamicgraph::Signal<T, int> Signal_t;
      typedef boost::shared_ptr<Signal_t> SignalPtr_t;
      typedef std::vector<T> buffer_t;
      typedef typename buffer_t::size_type size_type;

      BindedSignal(RosQueuedSubscribe* e)
        : BindedSignalBase (e)
        , frontIdx(0)
        , backIdx(0)
        , buffer (BufferSize)
        , init(false)
      {}
      ~BindedSignal()
      {
        signal.reset();
        clear();
      }

      /// See comments in reader and writer for details about synchronisation.
      void clear ()
      {
        // synchronize with method writer
        wmutex.lock();
        if (!empty()) {
          if (backIdx == 0)
            last = buffer[BufferSize-1];
          else
            last = buffer[backIdx-1];
        }
        // synchronize with method reader
        rmutex.lock();
        frontIdx = backIdx = 0;
        rmutex.unlock();
        wmutex.unlock();
      }

      bool empty () const
      {
        return frontIdx == backIdx;
      }

      bool full () const
      {
        return ((backIdx + 1) % BufferSize) == frontIdx;
      }

      size_type size () const
      {
        if (frontIdx <= backIdx)
          return backIdx - frontIdx;
        else
          return backIdx + BufferSize - frontIdx;
      }

      SignalPtr_t signal;
      /// Index of the next value to be read.
      size_type frontIdx;
      /// Index of the slot where to write next value (does not contain valid data).
      size_type backIdx;
      buffer_t buffer;
      boost::mutex wmutex, rmutex;
      T last;
      bool init;

      template <typename R> void writer (const R& data);
      T& reader (T& val, int time);
    };
  } // end of internal namespace.


  /// \brief Publish ROS information in the dynamic-graph.
  class RosQueuedSubscribe : public dynamicgraph::Entity
  {
    DYNAMIC_GRAPH_ENTITY_DECL();
    typedef boost::posix_time::ptime ptime;
  public:
    typedef boost::shared_ptr<internal::BindedSignalBase> bindedSignal_t;

    RosQueuedSubscribe (const std::string& n);
    virtual ~RosQueuedSubscribe ();

    virtual std::string getDocString () const;
    void display (std::ostream& os) const;

    void rm (const std::string& signal);
    std::string list ();
    void clear ();
    void clearQueue (const std::string& signal);
    void readQueue (int beginReadingAt);
    std::size_t queueSize (const std::string& signal) const;

    template <typename T>
    void add (const std::string& type, const std::string& signal, const std::string& topic);

    std::map<std::string, bindedSignal_t>&
    bindedSignal ()
    {
      return bindedSignal_;
    }

    ros::NodeHandle& nh ()
    {
      return nh_;
    }

    template <typename R, typename S>
    void callback
    (boost::shared_ptr<dynamicgraph::SignalPtr<S, int> > signal,
     const R& data);

    template <typename R>
    void callbackTimestamp
    (boost::shared_ptr<dynamicgraph::SignalPtr<ptime, int> > signal,
     const R& data);

    template <typename T>
    friend class internal::Add;
  private:
    static const std::string docstring_;
    ros::NodeHandle& nh_;
    std::map<std::string, bindedSignal_t> bindedSignal_;

    int readQueue_;
    // Signal<bool, int> readQueue_;

    template <typename T>
    friend class internal::BindedSignal;
  };
} // end of namespace dynamicgraph.

# include "ros_queued_subscribe.hxx"
#endif //! DYNAMIC_GRAPH_QUEUED_ROS_SUBSCRIBE_HH
