/*
 * An example to show usage of so_5::msg_tracing::individual_trace function.
 *
 * A message delivery tracing is enabled. Trace is going to std::cout.
 */

// Main SObjectizer header file.
#include <so_5/all.hpp>

using namespace std::chrono_literals;

// Signals for ping-pong.
struct ping final : public so_5::signal_t {};
struct pong final : public so_5::signal_t {};

// A class for implementation of pinger and ponger agents.
template< typename Signal_To_Send, typename Signal_To_Receive, bool Use_Individual_Trace >
class a_pinger_ponger_t final : public so_5::agent_t
{
	const so_5::mbox_t m_mbox;

public :
	a_pinger_ponger_t( context_t ctx, so_5::mbox_t mbox )
		:	so_5::agent_t( std::move(ctx) )
		,	m_mbox( std::move(mbox) )
	{}

	void so_define_agent() override
	{
		so_subscribe( m_mbox ).event( [this](mhood_t<Signal_To_Receive>) {
				if constexpr( Use_Individual_Trace )
					// Use individual tracing for outgoing messages.
					so_5::send_delayed< Signal_To_Send >(
							so_5::msg_tracing::individual_trace( m_mbox ), 25ms );
				else
					// Use the usual delivery.
					so_5::send_delayed< Signal_To_Send >( m_mbox, 25ms );
			} );
	}
};

// Main example agent.
class a_example_t final : public so_5::agent_t
{
	// Signal for finishing the example.
	struct finish final : public so_5::signal_t {};

public :
	a_example_t( context_t ctx )
		:	so_5::agent_t( std::move(ctx) )
	{
		so_subscribe_self().event( &a_example_t::on_finish );
	}

	virtual void so_evt_start() override
	{
		// Limit the work time.
		so_5::send_delayed< finish >( *this, std::chrono::milliseconds(500) );

		// Create a ping-pong pair which will work on separate thread.
		make_ping_pong_pair(
			so_5::disp::one_thread::make_dispatcher(
					so_environment() ).binder() );
	}

private :
	void on_finish( mhood_t<finish> )
	{
		so_deregister_agent_coop_normally();
	}

	void make_ping_pong_pair( so_5::disp_binder_shptr_t binder )
	{
		// A mbox to be used by pinger and ponger agents.
		const auto mbox = so_environment().create_mbox();

		// Create a new coop with two agents inside.
		so_5::introduce_child_coop( *this, std::move(binder),
			[mbox]( so_5::coop_t & coop ) {
				// Only pinger will use individual_trace.
				coop.make_agent< a_pinger_ponger_t<ping, pong, true> >( mbox );
				coop.make_agent< a_pinger_ponger_t<pong, ping, false> >( mbox );
			} );

		// Initiate ping-pong exchange.
		// Enable msg_tracing for this initial ping signal too.
		so_5::send_delayed< ping >( so_5::msg_tracing::individual_trace(mbox), 25ms );
	}
};

int main()
{
	try
	{
		so_5::launch( []( so_5::environment_t & env ) {
				env.introduce_coop( []( so_5::coop_t & coop ) {
					coop.make_agent< a_example_t >();
				} );
			},
			[]( so_5::environment_params_t & params ) {
				// Turn message delivery tracing on.
				params.message_delivery_tracer(
						so_5::msg_tracing::std_cout_tracer() );
				// Setup filter that allows only messages with individual trace.
				params.message_delivery_tracer_filter(
						so_5::msg_tracing::make_individual_trace_filter() );
			} );
	}
	catch( const std::exception & ex )
	{
		std::cerr << "Error: " << ex.what() << std::endl;
		return 1;
	}

	return 0;
}

