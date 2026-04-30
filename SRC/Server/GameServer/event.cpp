#include "stdafx.h"
#include <Core/Logging.hpp>

#include "event_queue.h"

extern void ContinueOnFatalError();
extern void ShutdownOnFatalError();
static CEventQueue cxx_q;

LPEVENT event_create_ex(TEVENTFUNC func, event_info_data* info, int32_t when)
{
	LPEVENT new_event = nullptr;

	if (when < 1)
		when = 1;

	new_event = M2_NEW event;

	assert(NULL != new_event);

	new_event->func = func;
	new_event->info	= info;
	new_event->q_el	= cxx_q.Enqueue(new_event, when, thecore_heart->pulse);
	new_event->is_processing = false;
	new_event->is_force_to_end = false;

	return (new_event);
}

void event_cancel(LPEVENT * ppevent)
{
	LPEVENT event;

	if (!ppevent)
	{
		LOG_ERROR("null pointer");
		return;
	}

	if (!(event = *ppevent))
		return;

	if (event->is_processing)
	{
		event->is_force_to_end = true;

		if (event->q_el)
			event->q_el->bCancel = true;

		*ppevent = nullptr;
		return;
	}

	if (!event->q_el)
	{
		*ppevent = nullptr;
		return;
	}

	if (event->q_el->bCancel)
	{
		*ppevent = nullptr;
		return;
	}

	event->q_el->bCancel = true;

	*ppevent = nullptr;
}

void event_reset_time(LPEVENT event, int32_t when)
{
	if (!event->is_processing)
	{
		if (event->q_el)
			event->q_el->bCancel = true;

		event->q_el = cxx_q.Enqueue(event, when, thecore_heart->pulse);
	}
}

int event_process(int pulse)
{
	int		num_events = 0;

	while (pulse >= cxx_q.GetTopKey())
	{
		TQueueElement * pElem = cxx_q.Dequeue();

		if (pElem->bCancel)
		{
			cxx_q.Delete(pElem);
			continue;
		}

		int32_t new_time = pElem->iKey;

		LPEVENT the_event = pElem->pvData;
		int32_t processing_time = event_processing_time(the_event);
		cxx_q.Delete(pElem);

		the_event->is_processing = true;

		if (!the_event->info)
		{
			the_event->q_el = nullptr;
			ContinueOnFatalError();
		}
		else
		{
			//0, "EVENT: %s %d event %p info %p", the_event->file, the_event->line, the_event, the_event->info);
			new_time = (the_event->func) (get_pointer(the_event), processing_time);

			if (new_time <= 0 || the_event->is_force_to_end)
			{
				the_event->q_el = nullptr;
			}
			else
			{
				the_event->q_el = cxx_q.Enqueue(the_event, new_time, pulse);
				the_event->is_processing = false;
			}
		}

		++num_events;
	}

	return num_events;
}

int32_t event_processing_time(LPEVENT event)
{
	if (!event->q_el)
		return 0;

	int32_t start_time = event->q_el->iStartTime;
	return (thecore_heart->pulse - start_time);
}

int32_t event_time(LPEVENT event)
{
	if (!event->q_el)
		return 0;

	int32_t when = event->q_el->iKey;
	return (when - thecore_heart->pulse);
}

void event_destroy(void)
{
	TQueueElement * pElem;

	while ((pElem = cxx_q.Dequeue()))
	{
		LPEVENT the_event = (LPEVENT) pElem->pvData;

		if (!pElem->bCancel)
		{
			// no op here
		}

		cxx_q.Delete(pElem);
	}
}

int event_count()
{
	return cxx_q.Size();
}

void intrusive_ptr_add_ref(EVENT* p) {
	++(p->ref_count);
}

void intrusive_ptr_release(EVENT* p) {
	if ( --(p->ref_count) == 0 ) {
		M2_DELETE(p);
	}
}
