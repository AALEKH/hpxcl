// Copyright (c)       2013 Martin Stumpf
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#ifndef HPX_OPENCL_LCOS_EVENT_HPP_
#define HPX_OPENCL_LCOS_EVENT_HPP_

#include <hpx/hpx.hpp>
#include <hpx/config.hpp>

#include <hpx/lcos/promise.hpp>

namespace hpx { namespace opencl { namespace lcos { namespace detail
{
    template <typename Result, typename RemoteResult>
    class event;
}}}}
 
///////////////////////////////////////////////////////////////////////////////
namespace hpx { namespace components { namespace detail
{
    // use the promise heap factory for constructing events
    template <typename Result, typename RemoteResult>
    struct heap_factory<
            opencl::lcos::detail::event<Result, RemoteResult>,
            managed_component<opencl::lcos::detail::event<Result, RemoteResult> > >
        : promise_heap_factory<opencl::lcos::detail::event<Result, RemoteResult> >
    {};
}}}


///////////////////////////////////////////////////////////////////////////////
namespace hpx { namespace opencl { namespace lcos { namespace detail
{

    ///////////////////////////////////////////////////////////////////////////

    void unregister_event( hpx::naming::id_type device_id,
                           hpx::naming::gid_type event_gid );

    ///////////////////////////////////////////////////////////////////////////
    template <typename Result, typename RemoteResult>
    class event
      : public hpx::lcos::detail::promise<Result, RemoteResult>
    {

    private:
        typedef hpx::lcos::detail::promise<Result, RemoteResult> parent_type;
        typedef typename hpx::lcos::detail::future_data<Result>::data_type
            data_type; 

    public:
       
        event(hpx::naming::id_type && device_id_, Result && result_buffer_)
            : device_id(std::move(device_id_)),
              result_buffer(std::move(result_buffer_))
        {
        }

        virtual
        ~event()
        {
            std::cout << "destroying event ..." << std::endl;
            unregister_event( device_id, 
                              this->get_base_gid() );
            std::cout << "event destroyed!" << std::endl;
        }

        ////////////////////////////////////////////////////////////////////////
        // Overrides that enable zero-copy
        //
    public:
        // This function is here for zero-copy of read_to_userbuffer_remote
        // Take (unmanaged) remote value for de-serialization (de-serialization
        // of the remote object automatically sets the value in result_buffer).
        // Then trigger set result_buffer as data of this event.

        // This holds the buffer that will get returned by the future.
        data_type result_buffer;

        
        ////////////////////////////////////////////////////////////////////////
        // Internal stuff
        //
    private:

        hpx::naming::id_type device_id;


        ////////////////////////////////////////////////////////////////////////
        // HPX Stuff
        //
    public:
        enum { value = components::component_promise };

    private:
        template <typename>
        friend struct components::detail_adl_barrier::init;

        void set_back_ptr(components::managed_component<event>* bp)
        {
            HPX_ASSERT(bp);
            HPX_ASSERT(this->gid_ == naming::invalid_gid);
            this->gid_ = bp->get_base_gid();
        }
    };

    ///////////////////////////////////////////////////////////////////////////
    template <>
    class event<void, hpx::util::unused_type>
      : public hpx::lcos::detail::promise<void, hpx::util::unused_type>
    {

    private:
        typedef hpx::lcos::detail::promise<void, hpx::util::unused_type>
            parent_type;
        typedef typename hpx::lcos::detail::future_data<void>::data_type
            data_type; 

    public:
       
        event(hpx::naming::id_type && device_id_)
            : device_id(std::move(device_id_)), is_armed(false)
        {
        }

        virtual
        ~event()
        {
            std::cout << "destroying event ..." << std::endl;
            unregister_event( device_id, 
                              this->get_base_gid() );
            std::cout << "event destroyed!" << std::endl;
        }

        ////////////////////////////////////////////////////////////////////////
        // Overrides that enable the event to be deferred
        //
    private:
        boost::atomic<bool> is_armed;

        void arm();

    public:
        // Gets called by when_all, wait_all, etc
        virtual void execute_deferred(error_code& ec = throws){
            if(!is_armed.exchange(true)){
                this->arm(); 
            }
        }

        // retrieving the value
        virtual data_type& get_result(error_code& ec = throws)
        {
            this->execute_deferred();
            return this->parent_type::get_result(ec);
        }

        // wait for the value
        virtual void wait(error_code& ec = throws)
        {
            this->execute_deferred();
            this->parent_type::wait(ec);
        }

        virtual BOOST_SCOPED_ENUM(hpx::lcos::future_status)
        wait_until(boost::chrono::steady_clock::time_point const& abs_time,
            error_code& ec = throws)
        {
            if (!is_armed.load())
                return hpx::lcos::future_status::deferred; //-V110
            else
                return this->parent_type::wait_until(abs_time, ec);
        };
        
        ////////////////////////////////////////////////////////////////////////
        // Internal stuff
        //
    private:

        hpx::naming::id_type device_id;


        ////////////////////////////////////////////////////////////////////////
        // HPX Stuff
        //
    public:
        enum { value = components::component_promise };

    private:
        template <typename>
        friend struct components::detail_adl_barrier::init;

        void set_back_ptr(components::managed_component<event>* bp)
        {
            HPX_ASSERT(bp);
            HPX_ASSERT(this->gid_ == naming::invalid_gid);
            this->gid_ = bp->get_base_gid();
        }
    };

}}}}

///////////////////////////////////////////////////////////////////////////////
namespace hpx { namespace traits
{
    template <typename Result, typename RemoteResult>
    struct managed_component_dtor_policy<
        opencl::lcos::detail::event<Result, RemoteResult> >
    {
        typedef managed_object_is_lifetime_controlled type;
    };
}}

///////////////////////////////////////////////////////////////////////////////
namespace hpx { namespace opencl { namespace lcos
{
    template <typename Result,
        typename RemoteResult =
            typename traits::promise_remote_result<Result>::type>
    class event;

    ///////////////////////////////////////////////////////////////////////////
    template <typename Result, typename RemoteResult>
    class event
    {
    public:
        typedef detail::event<Result, RemoteResult> wrapped_type;
        typedef components::managed_component<wrapped_type> wrapping_type;

    public:
        /// Construct a new \a event instance. The supplied
        /// \a thread will be notified as soon as the result of the
        /// operation associated with this future instance has been
        /// returned.
        ///
        /// \note         The result of the requested operation is expected to
        ///               be returned as the first parameter using a
        ///               \a base_lco#set_value action. Any error has to be
        ///               reported using a \a base_lco::set_exception action. The
        ///               target for either of these actions has to be this
        ///               future instance (as it has to be sent along
        ///               with the action as the continuation parameter).
        event(hpx::naming::id_type device_id, Result result_buffer = Result())
          : impl_(new wrapping_type(
                new wrapped_type(std::move(device_id), std::move(result_buffer))
            )),
            future_obtained_(false)
        {
            LLCO_(info) << "event::event(" << impl_->get_gid() << ")";
        }

    protected:
        template <typename Impl>
        event(Impl* impl)
          : impl_(impl), future_obtained_(false)
        {}

    public:
        /// Reset the event to allow to restart an asynchronous
        /// operation. Allows any subsequent set_data operation to succeed.
        void reset()
        {
            (*impl_)->reset();
            future_obtained_ = false;
        }

        /// \brief Return the global id of this \a future instance
        naming::id_type get_gid() const
        {
            return (*impl_)->get_gid();
        }

        /// \brief Return the global id of this \a future instance
        naming::gid_type get_base_gid() const
        {
            return (*impl_)->get_base_gid();
        }

        /// Return whether or not the data is available for this
        /// \a event.
        bool is_ready() const
        {
            return (*impl_)->is_ready();
        }

        /// Return whether this instance has been properly initialized
        bool valid() const
        {
            return impl_;
        }

        typedef Result result_type;

        virtual ~event()
        {}

        hpx::lcos::future<Result> get_future(error_code& ec = throws)
        {
            if (future_obtained_) {
                HPX_THROWS_IF(ec, future_already_retrieved,
                    "event<Result>::get_future",
                    "future already has been retrieved from this packaged_action");
                return hpx::lcos::future<Result>();
            }

            future_obtained_ = true;

            using traits::future_access;
            return future_access<future<Result> >::create(impl_->get());
        }

    protected:
        boost::intrusive_ptr<wrapping_type> impl_;
        bool future_obtained_;
    };

    ///////////////////////////////////////////////////////////////////////////
    template <>
    class event<void, hpx::util::unused_type>
    {
    public:
        typedef detail::event<void, hpx::util::unused_type> wrapped_type;
        typedef components::managed_component<wrapped_type> wrapping_type;

        /// Construct a new \a event instance. The supplied
        /// \a thread will be notified as soon as the result of the
        /// operation associated with this future instance has been
        /// returned.
        ///
        /// \note         The result of the requested operation is expected to
        ///               be returned as the first parameter using a
        ///               \a base_lco#set_value action. Any error has to be
        ///               reported using a \a base_lco::set_exception action. The
        ///               target for either of these actions has to be this
        ///               future instance (as it has to be sent along
        ///               with the action as the continuation parameter).
        event(hpx::naming::id_type device_id)
          : impl_(new wrapping_type( // event<void> is deferred
                new wrapped_type(std::move(device_id))
            )),
            future_obtained_(false)
        {
            LLCO_(info) << "event<void>::event(" << impl_->get_gid() << ")";
        }

    protected:
        template <typename Impl>
        event(Impl* impl)
          : impl_(impl), future_obtained_(false)
        {}

    public:
        /// Reset the event to allow to restart an asynchronous
        /// operation. Allows any subsequent set_data operation to succeed.
        void reset()
        {
            (*impl_)->reset();
            future_obtained_ = false;
        }

        /// \brief Return the global id of this \a future instance
        naming::id_type get_gid() const
        {
            return (*impl_)->get_gid();
        }

        /// \brief Return the global id of this \a future instance
        naming::gid_type get_base_gid() const
        {
            return (*impl_)->get_base_gid();
        }

        /// Return whether or not the data is available for this
        /// \a event.
        bool is_ready() const
        {
            return (*impl_)->is_ready();
        }

        typedef hpx::util::unused_type result_type;

        ~event()
        {}

        hpx::lcos::future<void> get_future(error_code& ec = throws)
        {
            if (future_obtained_) {
                HPX_THROWS_IF(ec, future_already_retrieved,
                    "event<void>::get_future",
                    "future already has been retrieved from this packaged_action");
                return hpx::lcos::future<void>();
            }

            future_obtained_ = true;

            using traits::future_access;
            return future_access<future<void> >::create(impl_->get());
        }

    protected:
        boost::intrusive_ptr<wrapping_type> impl_;
        bool future_obtained_;
    };
}}};

///////////////////////////////////////////////////////////////////////////////
namespace hpx { namespace traits
{
    namespace detail
    {
        HPX_EXPORT extern boost::detail::atomic_count unique_type;
    }

    template <typename Result, typename RemoteResult>
    struct component_type_database<
        hpx::opencl::lcos::detail::event<Result, RemoteResult> >
    {
        static components::component_type value;

        static components::component_type get()
        {
            // Events are never created remotely, their factories are not
            // registered with AGAS, so we can assign the component types
            // locally.
            if (value == components::component_invalid)
            {
                value = derived_component_type(++detail::unique_type,
                    components::component_base_lco_with_value);
            }
            return value;
        }

        static void set(components::component_type t)
        {
            HPX_ASSERT(false);
        }
    };

    template <typename Result, typename RemoteResult>
    components::component_type component_type_database<
        hpx::opencl::lcos::detail::event<Result, RemoteResult>
    >::value = components::component_invalid;
}}

#endif
