/*****************************************************************************
 *
 * TimeTypesOps.c
 *	  Operators for time types.
 *
 * Portions Copyright (c) 2019, Esteban Zimanyi, Arthur Lesuisse, 
 * 		Universite Libre de Bruxelles
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *****************************************************************************/

#include <TemporalTypes.h>

/*****************************************************************************/
/* Function needed for GIST index */

PG_FUNCTION_INFO_V1(timestamp_to_period);

PGDLLEXPORT Datum
timestamp_to_period(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	Period *result = period_make(t, t, true, true);
	PG_RETURN_POINTER(result);
}

/*****************************************************************************/
/* contains? */

bool
contains_timestampset_timestamp_internal(TimestampSet *ts, TimestampTz t)
{
	int n = timestampset_find_timestamp(ts, t);
	if (n == -1)
		return false;
	return true;
}

PG_FUNCTION_INFO_V1(contains_timestampset_timestamp);

PGDLLEXPORT Datum
contains_timestampset_timestamp(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	bool result = contains_timestampset_timestamp_internal(ts, t);
	PG_FREE_IF_COPY(ts, 0);
	PG_RETURN_BOOL(result);
}

bool
contains_timestampset_timestampset_internal(TimestampSet *ts1, TimestampSet *ts2)
{
	/* Test whether the timespan of the two TimestampSet values overlap */
	Period *p1 = timestampset_bbox(ts1);
	Period *p2 = timestampset_bbox(ts2);
	if (!contains_period_period_internal(p1, p2))
		return false;

	for (int i = 0; i < ts2->count; i++)
	{
		TimestampTz t = timestampset_time_n(ts2, i);
		if (!contains_timestampset_timestamp_internal(ts1, t))
			return false;
	}
	return true;
}

PG_FUNCTION_INFO_V1(contains_timestampset_timestampset);

PGDLLEXPORT Datum
contains_timestampset_timestampset(PG_FUNCTION_ARGS)
{
	TimestampSet *ts1 = PG_GETARG_TIMESTAMPSET(0);
	TimestampSet *ts2 = PG_GETARG_TIMESTAMPSET(1);
	bool result = contains_timestampset_timestampset_internal(ts1, ts2);
	PG_FREE_IF_COPY(ts1, 0);
	PG_FREE_IF_COPY(ts2, 1);
	PG_RETURN_BOOL(result);
}

bool
contains_period_timestamp_internal(Period *p, TimestampTz t)
{
	int32		cmp;
	
	cmp = timestamp_cmp_internal(p->lower, t);
	if (cmp > 0)
		return false;
	if (cmp == 0 && !(p->lower_inc))
		return false;
	
	cmp = timestamp_cmp_internal(p->upper, t);
	if (cmp < 0)
		return false;
	if (cmp == 0 && !(p->upper_inc))
		return false;
	
	return true;
}

PG_FUNCTION_INFO_V1(contains_period_timestamp);

PGDLLEXPORT Datum
contains_period_timestamp(PG_FUNCTION_ARGS)
{
	Period	   *p = PG_GETARG_PERIOD(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	PG_RETURN_BOOL(contains_period_timestamp_internal(p, t));
}

bool
contains_period_timestampset_internal(Period *p, TimestampSet *ts)
{
	for (int i = 0; i < ts->count; i++)
	{
		TimestampTz t = timestampset_time_n(ts, i);
		if (!contains_period_timestamp_internal(p, t))
			return false;
	}
	return true;
}

PG_FUNCTION_INFO_V1(contains_period_timestampset);

PGDLLEXPORT Datum
contains_period_timestampset(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = contains_period_timestampset_internal(p, ts);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

bool
contains_period_period_internal(Period *p1, Period *p2)
{
	/* We must have lower1 <= lower2 and upper1 >= upper2 */
	if (period_cmp_bounds(p1->lower, p2->lower, true, true, 
			p1->lower_inc, p2->lower_inc) > 0)
		return false;
	if (period_cmp_bounds(p1->upper, p2->upper, false, false, 
			p1->upper_inc, p2->upper_inc) < 0)
		return false;

	return true;
}

PG_FUNCTION_INFO_V1(contains_period_period);

PGDLLEXPORT Datum
contains_period_period(PG_FUNCTION_ARGS)
{
	Period *p1 = PG_GETARG_PERIOD(0);
	Period *p2 = PG_GETARG_PERIOD(1);
	PG_RETURN_BOOL(contains_period_period_internal(p1, p2));
}

bool
contains_periodset_timestamp_internal(PeriodSet *ps, TimestampTz t)
{
	int n = periodset_find_timestamp(ps, t);
	if (n == -1)
		return false;
	return true;
}

PG_FUNCTION_INFO_V1(contains_periodset_timestamp);

PGDLLEXPORT Datum
contains_periodset_timestamp(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	bool result = contains_periodset_timestamp_internal(ps, t);
	PG_FREE_IF_COPY(ps, 0);
	PG_RETURN_BOOL(result);
}

bool
contains_periodset_timestampset_internal(PeriodSet *ps, TimestampSet *ts)
{
	/* Test whether the timespan of the two values overlap */
	Period *ts1 = periodset_bbox(ps);
	Period *ts2 = timestampset_bbox(ts);
	if (!contains_period_period_internal(ts1, ts2))
		return false;

	int i = 0, j = 0; 
	while (j < ts->count)
	{
		Period *p = periodset_per_n(ps, i);
		TimestampTz t = timestampset_time_n(ts, j);
		if (contains_period_timestamp_internal(p, t))
			j++;
		else
		{
			if (timestamp_cmp_internal(t, p->upper) > 0)
			{
				i++;
				if (i == ps->count)
					return false;
			}
			else
				return false;
		}
	}
	return true;
}

PG_FUNCTION_INFO_V1(contains_periodset_timestampset);

PGDLLEXPORT Datum
contains_periodset_timestampset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = contains_periodset_timestampset_internal(ps, ts);
	PG_FREE_IF_COPY(ps, 0);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

bool
contains_periodset_period_internal(PeriodSet *ps, Period *p)
{
	int n = periodset_find_timestamp(ps, p->lower);
	if (n == -1)
		return false;
	Period *p1 = periodset_per_n(ps, n);
	return contains_period_period_internal(p1, p);
}

PG_FUNCTION_INFO_V1(contains_periodset_period);

PGDLLEXPORT Datum
contains_periodset_period(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	bool result = contains_periodset_period_internal(ps, p);
	PG_FREE_IF_COPY(ps, 0);
	PG_RETURN_BOOL(result);
}

bool
contains_period_periodset_internal(Period *p, PeriodSet *ps)
{
	Period *ts = periodset_bbox(ps);
	bool result = contains_period_period_internal(p, ts);
	return result;
}

PG_FUNCTION_INFO_V1(contains_period_periodset);

PGDLLEXPORT Datum
contains_period_periodset(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = contains_period_periodset_internal(p, ps);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}

bool
contains_periodset_periodset_internal(PeriodSet *ps1, PeriodSet *ps2)
{
	/* Test whether the timespan of the two PeriodSet values overlap */
	Period *ts1 = periodset_bbox(ps1);
	Period *ts2 = periodset_bbox(ps2);
	if (!contains_period_period_internal(ts1, ts2))
		return false;

	for (int i = 0; i < ps2->count; i++)
	{
		Period *p = periodset_per_n(ps2, i);
		if (!contains_periodset_period_internal(ps1, p))
			return false;
	}
	return true;
}

PG_FUNCTION_INFO_V1(contains_periodset_periodset);

PGDLLEXPORT Datum
contains_periodset_periodset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps1 = PG_GETARG_PERIODSET(0);
	PeriodSet *ps2 = PG_GETARG_PERIODSET(1);
	bool result = contains_periodset_periodset_internal(ps1, ps2);
	PG_FREE_IF_COPY(ps1, 0);
	PG_FREE_IF_COPY(ps2, 1);
	PG_RETURN_BOOL(result);
}

/*****************************************************************************/
/* contained? */

bool
contained_timestamp_timestampset_internal(TimestampTz t, TimestampSet *ts)
{
	PG_RETURN_BOOL(contains_timestampset_timestamp_internal(ts, t));
}

PG_FUNCTION_INFO_V1(contained_timestamp_timestampset);

PGDLLEXPORT Datum
contained_timestamp_timestampset(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = contains_timestampset_timestamp_internal(ts, t);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

bool
contained_timestamp_period_internal(TimestampTz t, Period *p)
{
	PG_RETURN_BOOL(contains_period_timestamp_internal(p, t));
}

PG_FUNCTION_INFO_V1(contained_timestamp_period);

PGDLLEXPORT Datum
contained_timestamp_period(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	Period *p = PG_GETARG_PERIOD(1);
	PG_RETURN_BOOL(contains_period_timestamp_internal(p, t));
}

bool
contained_timestamp_periodset_internal(TimestampTz t, PeriodSet *ps)
{
	PG_RETURN_BOOL(contains_periodset_timestamp_internal(ps, t));
}

PG_FUNCTION_INFO_V1(contained_timestamp_periodset);

PGDLLEXPORT Datum
contained_timestamp_periodset(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = contains_periodset_timestamp_internal(ps, t);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(contained_timestampset_timestampset);

PGDLLEXPORT Datum
contained_timestampset_timestampset(PG_FUNCTION_ARGS)
{
	TimestampSet *ts1 = PG_GETARG_TIMESTAMPSET(0);
	TimestampSet *ts2 = PG_GETARG_TIMESTAMPSET(1);
	bool result = contains_timestampset_timestampset_internal(ts2, ts1);
	PG_FREE_IF_COPY(ts1, 0);
	PG_FREE_IF_COPY(ts2, 1);
	PG_RETURN_BOOL(result);
}

bool
contained_timestampset_period_internal(TimestampSet *ts, Period *p)
{
	TimestampTz t1 = timestampset_time_n(ts, 0);
	TimestampTz t2 = timestampset_time_n(ts, ts->count-1);
	if (contains_period_timestamp_internal(p, t1) && 
		contains_period_timestamp_internal(p, t2))
		return true;
	return false;
}

PG_FUNCTION_INFO_V1(contained_timestampset_period);

PGDLLEXPORT Datum
contained_timestampset_period(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	bool result = contained_timestampset_period_internal(ts, p);
	PG_FREE_IF_COPY(ts, 0);
	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(contained_timestampset_periodset);

PGDLLEXPORT Datum
contained_timestampset_periodset(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = contains_periodset_timestampset_internal(ps, ts);
	PG_FREE_IF_COPY(ts, 0);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}

bool
contained_period_period_internal(Period *p1, Period *p2)
{
	return contains_period_period_internal(p2, p1);
}

PG_FUNCTION_INFO_V1(contained_period_period);

PGDLLEXPORT Datum
contained_period_period(PG_FUNCTION_ARGS)
{
	Period *p1 = PG_GETARG_PERIOD(0);
	Period *p2 = PG_GETARG_PERIOD(1);
	PG_RETURN_BOOL(contained_period_period_internal(p1, p2));
}

PG_FUNCTION_INFO_V1(contained_period_periodset);

PGDLLEXPORT Datum
contained_period_periodset(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = contains_periodset_period_internal(ps, p);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}

bool
contained_periodset_period_internal(PeriodSet *ps, Period *p)
{
	Period *p1 = periodset_per_n(ps, 0);
	Period *p2 = periodset_per_n(ps, ps->count-1);
	/* We must have p->lower <= p1.lower and p->upper >= p2->upper */
	if (period_cmp_bounds(p->lower, p1->lower, true, true, 
			p->lower_inc, p1->lower_inc) <= 0 &&
		period_cmp_bounds(p->upper, p2->upper, false, false, 
			p->upper_inc, p2->upper_inc) >= 0)
		return true;
	return false;
}

PG_FUNCTION_INFO_V1(contained_periodset_period);

PGDLLEXPORT Datum
contained_periodset_period(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	bool result = contained_periodset_period_internal(ps, p);
	PG_FREE_IF_COPY(ps, 0);
	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(contained_periodset_periodset);

PGDLLEXPORT Datum
contained_periodset_periodset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps1 = PG_GETARG_PERIODSET(0);
	PeriodSet *ps2 = PG_GETARG_PERIODSET(1);
	bool result = contains_periodset_periodset_internal(ps2, ps1);
	PG_FREE_IF_COPY(ps1, 0);
	PG_FREE_IF_COPY(ps2, 1);
	PG_RETURN_BOOL(result);
}

/*****************************************************************************/
/* overlaps? */

bool
overlaps_timestampset_timestampset_internal(TimestampSet *ts1, TimestampSet *ts2)
{
	/* Test whether the timespan of the two TimestampSet values overlap */
	Period *p1 = timestampset_bbox(ts1);
	Period *p2 = timestampset_bbox(ts2);
	if (!overlaps_period_period_internal(p1, p2))
		return false;
	
	int i = 0, j = 0;
	while (i < ts1->count && j < ts2->count)
	{
		TimestampTz t1 = timestampset_time_n(ts1, i);
		TimestampTz t2 = timestampset_time_n(ts2, j);
		if (timestamp_cmp_internal(t1, t2) == 0)
			return true;
		if (timestamp_cmp_internal(t1, t2) < 0)
			i++;
		else 
			j++;
	}
	return false;
}

PG_FUNCTION_INFO_V1(overlaps_timestampset_timestampset);

PGDLLEXPORT Datum
overlaps_timestampset_timestampset(PG_FUNCTION_ARGS)
{
	TimestampSet *ts1 = PG_GETARG_TIMESTAMPSET(0);
	TimestampSet *ts2 = PG_GETARG_TIMESTAMPSET(1);
	bool result = overlaps_timestampset_timestampset_internal(ts1, ts2);
	PG_FREE_IF_COPY(ts1, 0);
	PG_FREE_IF_COPY(ts2, 1);
	PG_RETURN_BOOL(result);
}

bool
overlaps_timestampset_period_internal(TimestampSet *ts, Period *p)
{
	/* Does the timespan of the timestamp set overlaps the period */
	Period *p1 = timestampset_bbox(ts);
	if (!overlaps_period_period_internal(p, p1))
		return false;
	
	for (int i = 0; i < ts->count; i++)
	{
		TimestampTz t = timestampset_time_n(ts, i);
		if (contains_period_timestamp_internal(p, t))
			return true;
	}
	return false;
}

PG_FUNCTION_INFO_V1(overlaps_timestampset_period);

PGDLLEXPORT Datum
overlaps_timestampset_period(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	bool result = overlaps_timestampset_period_internal(ts, p);
	PG_FREE_IF_COPY(ts, 0);
	PG_RETURN_BOOL(result);
}

bool
overlaps_timestampset_periodset_internal(TimestampSet *ts, PeriodSet *ps)
{
	/* Does the timespan of the timestamp set overlaps the period set */
	Period *p1 = timestampset_bbox(ts);
	Period *p2 = periodset_bbox(ps);
	if (!overlaps_period_period_internal(p1, p2))
		return false;
	
	for (int i = 0; i < ts->count; i++)
	{
		TimestampTz t = timestampset_time_n(ts, i);
		if (contains_periodset_timestamp_internal(ps, t))
			return true;
	}
	return false;
}

PG_FUNCTION_INFO_V1(overlaps_timestampset_periodset);

PGDLLEXPORT Datum
overlaps_timestampset_periodset(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = overlaps_timestampset_periodset_internal(ts, ps);
	PG_FREE_IF_COPY(ts, 0);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(overlaps_period_timestampset);

PGDLLEXPORT Datum
overlaps_period_timestampset(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = overlaps_timestampset_period_internal(ts, p);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

bool
overlaps_period_period_internal(Period *p1, Period *p2)
{
	if (period_cmp_bounds(p1->lower, p2->lower, true, true, 
			p1->lower_inc, p2->lower_inc) >= 0 &&
		period_cmp_bounds(p1->lower, p2->upper, true, false, 
			p1->lower_inc, p2->upper_inc) <= 0)
		return true;

	if (period_cmp_bounds(p2->lower, p1->lower, true, true, 
			p2->lower_inc, p1->lower_inc) >= 0 &&
		period_cmp_bounds(p2->lower, p1->upper, true, false, 
			p2->lower_inc, p1->upper_inc) <= 0)
		return true;

	return false;
}

PG_FUNCTION_INFO_V1(overlaps_period_period);

PGDLLEXPORT Datum
overlaps_period_period(PG_FUNCTION_ARGS)
{
	Period *p1 = PG_GETARG_PERIOD(0);
	Period *p2 = PG_GETARG_PERIOD(1);
	PG_RETURN_BOOL(overlaps_period_period_internal(p1, p2));
}

PG_FUNCTION_INFO_V1(overlaps_period_periodset);

bool
overlaps_period_periodset_internal(Period *p, PeriodSet *ps)
{
	/* Binary search of lower and upper bounds of period */
	int n1 = periodset_find_timestamp(ps, p->lower);
	int n2 = periodset_find_timestamp(ps, p->upper);
	/* It may be the case the period n1/n2 has non inclusive upper/lower 
	   bound  equal to p->lower/p->upper */
	if ((n1 != -1 && p->lower_inc) || 
		(n2 != -1 && p->upper_inc))
		return true;
	
	for (int i = 0; i < ps->count; i++)
	{
		Period *p1 = periodset_per_n(ps, i);
		if (overlaps_period_period_internal(p1, p))
			return true;
		if (timestamp_cmp_internal(p->upper, p1->upper) < 0)
			break;
	}
	return false;
}

PGDLLEXPORT Datum
overlaps_period_periodset(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = overlaps_period_periodset_internal(p, ps);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}


PG_FUNCTION_INFO_V1(overlaps_periodset_timestampset);

PGDLLEXPORT Datum
overlaps_periodset_timestampset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = overlaps_timestampset_periodset_internal(ts, ps);
	PG_FREE_IF_COPY(ps, 0);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

PG_FUNCTION_INFO_V1(overlaps_periodset_period);

PGDLLEXPORT Datum
overlaps_periodset_period(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	bool result = overlaps_period_periodset_internal(p, ps);
	PG_FREE_IF_COPY(ps, 0);
	PG_RETURN_BOOL(result);
}

bool
overlaps_periodset_periodset_internal(PeriodSet *ps1, PeriodSet *ps2)
{
	/* Test whether the timespan of the two PeriodSet values overlap */
	Period *ts1 = periodset_bbox(ps1);
	Period *ts2 = periodset_bbox(ps2);
	if (!overlaps_period_period_internal(ts1, ts2))
		return false;
	
	int i = 0, j = 0;
	while (i < ps1->count && j < ps2->count)
	{
		Period *p1 = periodset_per_n(ps1, i);
		Period *p2 = periodset_per_n(ps2, j);
		if (overlaps_period_period_internal(p1, p2))
			return true;
		if (timestamp_cmp_internal(p1->upper, p2->upper) == 0)
		{
			i++; j++;
		}
		else if (timestamp_cmp_internal(p1->upper, p2->upper) < 0)
			i++;
		else 
			j++;
	}
	return false;
}

PG_FUNCTION_INFO_V1(overlaps_periodset_periodset);

PGDLLEXPORT Datum
overlaps_periodset_periodset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps1 = PG_GETARG_PERIODSET(0);
	PeriodSet *ps2 = PG_GETARG_PERIODSET(1);
	bool result = overlaps_periodset_periodset_internal(ps1, ps2);
	PG_FREE_IF_COPY(ps1, 0);
	PG_FREE_IF_COPY(ps2, 1);
	PG_RETURN_BOOL(result);
}

/*****************************************************************************/
/* strictly left of? */

bool
before_timestamp_timestampset_internal(TimestampTz t, TimestampSet *ts)
{
	TimestampTz t1 = timestampset_time_n(ts, 0);
	return (timestamp_cmp_internal(t, t1) < 0);
}

PG_FUNCTION_INFO_V1(before_timestamp_timestampset);

PGDLLEXPORT Datum
before_timestamp_timestampset(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = before_timestamp_timestampset_internal(t, ts);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

bool
before_timestamp_period_internal(TimestampTz t, Period *p)
{
	return (period_cmp_bounds(t, p->lower, false, true, 
		true, p->lower_inc) < 0);
}

PG_FUNCTION_INFO_V1(before_timestamp_period);

PGDLLEXPORT Datum
before_timestamp_period(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	Period *p = PG_GETARG_PERIOD(1);
	PG_RETURN_BOOL(before_timestamp_period_internal(t, p));
}

bool
before_timestamp_periodset_internal(TimestampTz t, PeriodSet *ps)
{
	Period *p = periodset_per_n(ps, 0);
	return (period_cmp_bounds(t, p->lower, false, true, 
		true, p->lower_inc) < 0);
}

PG_FUNCTION_INFO_V1(before_timestamp_periodset);

PGDLLEXPORT Datum
before_timestamp_periodset(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = before_timestamp_periodset_internal(t, ps);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}

bool
before_timestampset_timestamp_internal(TimestampSet *ts, TimestampTz t)
{	
	TimestampTz t1 = timestampset_time_n(ts, ts->count-1);
	return (timestamp_cmp_internal(t1, t) < 0);
}

PG_FUNCTION_INFO_V1(before_timestampset_timestamp);

PGDLLEXPORT Datum
before_timestampset_timestamp(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	bool result = before_timestampset_timestamp_internal(ts, t);
	PG_FREE_IF_COPY(ts, 0);
	PG_RETURN_BOOL(result);
}

bool
before_timestampset_timestampset_internal(TimestampSet *ts1, TimestampSet *ts2)
{
	TimestampTz t1 = timestampset_time_n(ts1, ts1->count-1);
	TimestampTz t2 = timestampset_time_n(ts2, 0);
	return (timestamp_cmp_internal(t1, t2) < 0);
}

PG_FUNCTION_INFO_V1(before_timestampset_timestampset);

PGDLLEXPORT Datum
before_timestampset_timestampset(PG_FUNCTION_ARGS)
{
	TimestampSet *ts1 = PG_GETARG_TIMESTAMPSET(0);
	TimestampSet *ts2 = PG_GETARG_TIMESTAMPSET(1);
	bool result = before_timestampset_timestampset_internal(ts1, ts2);
	PG_FREE_IF_COPY(ts1, 0);
	PG_FREE_IF_COPY(ts2, 1);
	PG_RETURN_BOOL(result);
}

bool
before_timestampset_period_internal(TimestampSet *ts, Period *p)
{
	TimestampTz t = timestampset_time_n(ts, ts->count-1);
	return before_timestamp_period_internal(t, p);
}

PG_FUNCTION_INFO_V1(before_timestampset_period);

PGDLLEXPORT Datum
before_timestampset_period(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	bool result = before_timestampset_period_internal(ts, p);
	PG_FREE_IF_COPY(ts, 0);
	PG_RETURN_BOOL(result);
}

bool
before_timestampset_periodset_internal(TimestampSet *ts, PeriodSet *ps)
{
	Period *p = periodset_per_n(ps, 0);
	TimestampTz t = timestampset_time_n(ts, ts->count-1);
	return (period_cmp_bounds(p->lower, t, true, false, 
		p->lower_inc, true) > 0);		
}

PG_FUNCTION_INFO_V1(before_timestampset_periodset);

PGDLLEXPORT Datum
before_timestampset_periodset(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = before_timestampset_periodset_internal(ts, ps);
	PG_FREE_IF_COPY(ts, 0);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}

bool
before_period_timestamp_internal(Period *p, TimestampTz t)
{
	return (period_cmp_bounds(p->upper, t, false, true, 
		p->upper_inc, true) < 0);
}

PG_FUNCTION_INFO_V1(before_period_timestamp);

PGDLLEXPORT Datum
before_period_timestamp(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	PG_RETURN_BOOL(before_period_timestamp_internal(p, t));
}

bool
before_period_timestampset_internal(Period *p, TimestampSet *ts)
{
	TimestampTz t = timestampset_time_n(ts, 0);
	return before_period_timestamp_internal(p, t);
}

PG_FUNCTION_INFO_V1(before_period_timestampset);

PGDLLEXPORT Datum
before_period_timestampset(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = before_period_timestampset_internal(p, ts);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

bool
before_period_period_internal(Period *p1, Period *p2)
{
	return (period_cmp_bounds(p1->upper, p2->lower, false, true, 
		p1->upper_inc, p2->lower_inc) < 0);
}

PG_FUNCTION_INFO_V1(before_period_period);

PGDLLEXPORT Datum
before_period_period(PG_FUNCTION_ARGS)
{
	Period *p1 = PG_GETARG_PERIOD(0);
	Period *p2 = PG_GETARG_PERIOD(1);
	PG_RETURN_BOOL(before_period_period_internal(p1, p2));
}

bool
before_period_periodset_internal(Period *p, PeriodSet *ps)
{
	Period *p1 = periodset_per_n(ps, 0);
	return (period_cmp_bounds(p->upper, p1->lower, false, true, 
		p->upper_inc, p1->lower_inc) < 0);
}

PG_FUNCTION_INFO_V1(before_period_periodset);

PGDLLEXPORT Datum
before_period_periodset(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = before_period_periodset_internal(p, ps);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}

bool
before_periodset_timestamp_internal(PeriodSet *ps, TimestampTz t)
{	
	Period *p = periodset_per_n(ps, ps->count-1);
	return (period_cmp_bounds(p->upper, t, false, true, 
		p->upper_inc, true) < 0);
}

PG_FUNCTION_INFO_V1(before_periodset_timestamp);

PGDLLEXPORT Datum
before_periodset_timestamp(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	bool result = before_periodset_timestamp_internal(ps, t);
	PG_FREE_IF_COPY(ps, 0);
	PG_RETURN_BOOL(result);
}

bool
before_periodset_timestampset_internal(PeriodSet *ps, TimestampSet *ts)
{
	Period *p = periodset_per_n(ps, ps->count-1);
	TimestampTz t = timestampset_time_n(ts, 0);
	return (period_cmp_bounds(p->upper, t, false, true, 
		p->upper_inc, true) < 0);	
}

PG_FUNCTION_INFO_V1(before_periodset_timestampset);

PGDLLEXPORT Datum
before_periodset_timestampset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = before_periodset_timestampset_internal(ps, ts);
	PG_FREE_IF_COPY(ps, 0);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

bool
before_periodset_period_internal(PeriodSet *ps, Period *p)
{
	Period *p1 = periodset_per_n(ps, ps->count-1);
	return (period_cmp_bounds(p1->upper, p->lower, false, true, 
		p1->upper_inc, p->lower_inc) < 0);
}

PG_FUNCTION_INFO_V1(before_periodset_period);

PGDLLEXPORT Datum
before_periodset_period(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	bool result = before_periodset_period_internal(ps, p);
	PG_FREE_IF_COPY(ps, 0);
	PG_RETURN_BOOL(result);
}

bool
before_periodset_periodset_internal(PeriodSet *ps1, PeriodSet *ps2)
{
	Period *p1 = periodset_per_n(ps1, ps1->count-1);
	Period *p2 = periodset_per_n(ps2, 0);
	return (period_cmp_bounds(p1->upper, p2->lower, false, true, 
		p1->upper_inc, p2->lower_inc) < 0);
}

PG_FUNCTION_INFO_V1(before_periodset_periodset);

PGDLLEXPORT Datum
before_periodset_periodset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps1 = PG_GETARG_PERIODSET(0);
	PeriodSet *ps2 = PG_GETARG_PERIODSET(1);
	bool result = before_periodset_periodset_internal(ps1, ps2);
	PG_FREE_IF_COPY(ps1, 0);
	PG_FREE_IF_COPY(ps2, 1);
	PG_RETURN_BOOL(result);
}

/*****************************************************************************/
/* strictly right of? */

bool
after_timestamp_timestampset_internal(TimestampTz t, TimestampSet *ts)
{
	TimestampTz t1 = timestampset_time_n(ts, ts->count-1);
	return (timestamp_cmp_internal(t, t1) > 0);
}

PG_FUNCTION_INFO_V1(after_timestamp_timestampset);

PGDLLEXPORT Datum
after_timestamp_timestampset(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = after_timestamp_timestampset_internal(t, ts);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

bool
after_timestamp_period_internal(TimestampTz t, Period *p)
{
	return (period_cmp_bounds(t, p->upper, true, false, 
		true, p->upper_inc) > 0);
}

PG_FUNCTION_INFO_V1(after_timestamp_period);

PGDLLEXPORT Datum
after_timestamp_period(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	Period *p = PG_GETARG_PERIOD(1);
	PG_RETURN_BOOL(after_timestamp_period_internal(t, p));
}

bool
after_timestamp_periodset_internal(TimestampTz t, PeriodSet *ps)
{
	Period *p = periodset_per_n(ps, ps->count-1);
	return (period_cmp_bounds(t, p->upper, true, false, 
		true, p->upper_inc) > 0);
}

PG_FUNCTION_INFO_V1(after_timestamp_periodset);

PGDLLEXPORT Datum
after_timestamp_periodset(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = after_timestamp_periodset_internal(t, ps);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}


bool
after_timestampset_timestamp_internal(TimestampSet *ts, TimestampTz t)
{
	TimestampTz t1 = timestampset_time_n(ts, 0);
	return (timestamp_cmp_internal(t1, t) > 0);
}

PG_FUNCTION_INFO_V1(after_timestampset_timestamp);

PGDLLEXPORT Datum
after_timestampset_timestamp(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	bool result = after_timestampset_timestamp_internal(ts, t);
	PG_FREE_IF_COPY(ts, 0);
	PG_RETURN_BOOL(result);
}

bool
after_timestampset_timestampset_internal(TimestampSet *ts1, TimestampSet *ts2)
{
	TimestampTz t1 = timestampset_time_n(ts1, 0);
	TimestampTz t2 = timestampset_time_n(ts2, ts2->count-1);
	return (timestamp_cmp_internal(t1, t2) > 0);
}

PG_FUNCTION_INFO_V1(after_timestampset_timestampset);

PGDLLEXPORT Datum
after_timestampset_timestampset(PG_FUNCTION_ARGS)
{
	TimestampSet *ts1 = PG_GETARG_TIMESTAMPSET(0);
	TimestampSet *ts2 = PG_GETARG_TIMESTAMPSET(1);
	bool result = after_timestampset_timestampset_internal(ts1, ts2);
	PG_FREE_IF_COPY(ts1, 0);
	PG_FREE_IF_COPY(ts2, 1);
	PG_RETURN_BOOL(result);
}

/* strictly right of? */
bool
after_timestampset_period_internal(TimestampSet *ts, Period *p)
{
	TimestampTz t = timestampset_time_n(ts, 0);
	return after_timestamp_period_internal(t, p);
}

PG_FUNCTION_INFO_V1(after_timestampset_period);

PGDLLEXPORT Datum
after_timestampset_period(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	bool result = after_timestampset_period_internal(ts, p);
	PG_FREE_IF_COPY(ts, 0);
	PG_RETURN_BOOL(result);
}

bool
after_timestampset_periodset_internal(TimestampSet *ts, PeriodSet *ps)
{
	Period *p = periodset_per_n(ps, ps->count-1);
	TimestampTz t = timestampset_time_n(ts, 0);
	return (period_cmp_bounds(p->upper, t, false, true, 
		p->upper_inc, true) < 0);		
}

PG_FUNCTION_INFO_V1(after_timestampset_periodset);

PGDLLEXPORT Datum
after_timestampset_periodset(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = after_timestampset_periodset_internal(ts, ps);
	PG_FREE_IF_COPY(ts, 0);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}

bool
after_period_timestamp_internal(Period *p, TimestampTz t)
{
	return (period_cmp_bounds(p->lower, t, true, false, 
		p->lower_inc, true) > 0);
}

PG_FUNCTION_INFO_V1(after_period_timestamp);

PGDLLEXPORT Datum
after_period_timestamp(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	PG_RETURN_BOOL(after_period_timestamp_internal(p, t));
}

bool
after_period_timestampset_internal(Period *p, TimestampSet *ts)
{
	TimestampTz t = timestampset_time_n(ts, ts->count-1);
	return after_period_timestamp_internal(p, t);
}

PG_FUNCTION_INFO_V1(after_period_timestampset);

PGDLLEXPORT Datum
after_period_timestampset(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = after_period_timestampset_internal(p, ts);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

bool
after_period_period_internal(Period *p1, Period *p2)
{
	return (period_cmp_bounds(p1->lower, p2->upper, true, false, 
		p1->lower_inc, p2->upper_inc) > 0);
}

PG_FUNCTION_INFO_V1(after_period_period);

PGDLLEXPORT Datum
after_period_period(PG_FUNCTION_ARGS)
{
	Period *p1 = PG_GETARG_PERIOD(0);
	Period *p2 = PG_GETARG_PERIOD(1);
	PG_RETURN_BOOL(after_period_period_internal(p1, p2));
}

bool
after_period_periodset_internal(Period *p, PeriodSet *ps)
{
	Period *p1 = periodset_per_n(ps, ps->count-1);
	return (period_cmp_bounds(p->lower, p1->upper, true, false, 
		p->lower_inc, p1->upper_inc) > 0);
}

PG_FUNCTION_INFO_V1(after_period_periodset);

PGDLLEXPORT Datum
after_period_periodset(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = after_period_periodset_internal(p, ps);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}

bool
after_periodset_timestamp_internal(PeriodSet *ps, TimestampTz t)
{
	Period *p = periodset_per_n(ps, 0);
	return (period_cmp_bounds(p->lower, t, true, false, 
		p->lower_inc, true) > 0);
}

PG_FUNCTION_INFO_V1(after_periodset_timestamp);

PGDLLEXPORT Datum
after_periodset_timestamp(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	bool result = after_periodset_timestamp_internal(ps, t);
	PG_FREE_IF_COPY(ps, 0);
	PG_RETURN_BOOL(result);
}

bool
after_periodset_timestampset_internal(PeriodSet *ps, TimestampSet *ts)
{
	Period *p = periodset_per_n(ps, 0);
	TimestampTz t = timestampset_time_n(ts, ts->count-1);
	return (period_cmp_bounds(p->lower, t, true, false, 
		p->lower_inc, true) > 0);	
}

PG_FUNCTION_INFO_V1(after_periodset_timestampset);

PGDLLEXPORT Datum
after_periodset_timestampset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = after_periodset_timestampset_internal(ps, ts);
	PG_FREE_IF_COPY(ps, 0);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

bool
after_periodset_period_internal(PeriodSet *ps, Period *p)
{
	Period *p1 = periodset_per_n(ps, 0);
	return (period_cmp_bounds(p1->lower, p->upper, true, false, 
		p1->lower_inc, p->upper_inc) > 0);
}

PG_FUNCTION_INFO_V1(after_periodset_period);

PGDLLEXPORT Datum
after_periodset_period(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	bool result = after_periodset_period_internal(ps, p);
	PG_FREE_IF_COPY(ps, 0);
	PG_RETURN_BOOL(result);
}

bool
after_periodset_periodset_internal(PeriodSet *ps1, PeriodSet *ps2)
{
	Period *p1 = periodset_per_n(ps1, 0);
	Period *p2 = periodset_per_n(ps2, ps2->count-1);
	return (period_cmp_bounds(p1->lower, p2->upper, true, false, 
		p1->lower_inc, p2->upper_inc) > 0);
}

PG_FUNCTION_INFO_V1(after_periodset_periodset);

PGDLLEXPORT Datum
after_periodset_periodset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps1 = PG_GETARG_PERIODSET(0);
	PeriodSet *ps2 = PG_GETARG_PERIODSET(1);
	bool result = after_periodset_periodset_internal(ps1, ps2);
	PG_FREE_IF_COPY(ps1, 0);
	PG_FREE_IF_COPY(ps2, 1);
	PG_RETURN_BOOL(result);
}

/*****************************************************************************/
/* does not extend to right of? */

bool
overbefore_timestamp_timestampset_internal(TimestampTz t, TimestampSet *ts)
{
	TimestampTz t1 = timestampset_time_n(ts, ts->count-1);
	return (timestamp_cmp_internal(t, t1) <= 0);
}

PG_FUNCTION_INFO_V1(overbefore_timestamp_timestampset);

PGDLLEXPORT Datum
overbefore_timestamp_timestampset(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = overbefore_timestamp_timestampset_internal(t, ts);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

bool
overbefore_timestamp_period_internal(TimestampTz t, Period *p)
{
	return (period_cmp_bounds(t, p->upper, false, false, 
			true, p->upper_inc) <= 0);
}

PG_FUNCTION_INFO_V1(overbefore_timestamp_period);

PGDLLEXPORT Datum
overbefore_timestamp_period(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	Period *p = PG_GETARG_PERIOD(1);
	PG_RETURN_BOOL(overbefore_timestamp_period_internal(t, p));
}

bool
overbefore_timestamp_periodset_internal(TimestampTz t, PeriodSet *ps)
{
	Period *p = periodset_per_n(ps, ps->count-1);
	return (period_cmp_bounds(t, p->upper, false, false, 
			true, p->upper_inc) <= 0);
}

PG_FUNCTION_INFO_V1(overbefore_timestamp_periodset);

PGDLLEXPORT Datum
overbefore_timestamp_periodset(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = overbefore_timestamp_periodset_internal(t, ps);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}

bool
overbefore_timestampset_timestamp_internal(TimestampSet *ts, TimestampTz t)
{
	TimestampTz t1 = timestampset_time_n(ts, ts->count-1);
	return (timestamp_cmp_internal(t1, t) <= 0);
}

PG_FUNCTION_INFO_V1(overbefore_timestampset_timestamp);

PGDLLEXPORT Datum
overbefore_timestampset_timestamp(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	bool result = overbefore_timestampset_timestamp_internal(ts, t);
	PG_FREE_IF_COPY(ts, 0);
	PG_RETURN_BOOL(result);
}

bool
overbefore_timestampset_timestampset_internal(TimestampSet *ts1, TimestampSet *ts2)
{
	TimestampTz t1 = timestampset_time_n(ts1, ts1->count-1);
	TimestampTz t2 = timestampset_time_n(ts2, ts2->count-1);
	return (timestamp_cmp_internal(t1, t2) <= 0);
}

PG_FUNCTION_INFO_V1(overbefore_timestampset_timestampset);

PGDLLEXPORT Datum
overbefore_timestampset_timestampset(PG_FUNCTION_ARGS)
{
	TimestampSet *ts1 = PG_GETARG_TIMESTAMPSET(0);
	TimestampSet *ts2 = PG_GETARG_TIMESTAMPSET(1);
	bool result = overbefore_timestampset_timestampset_internal(ts1, ts2);
	PG_FREE_IF_COPY(ts1, 0);
	PG_FREE_IF_COPY(ts2, 1);
	PG_RETURN_BOOL(result);
}

bool
overbefore_timestampset_period_internal(TimestampSet *ts, Period *p)
{
	TimestampTz t = timestampset_time_n(ts, ts->count-1);
	return (overbefore_timestamp_period_internal(t, p));
}

PG_FUNCTION_INFO_V1(overbefore_timestampset_period);

PGDLLEXPORT Datum
overbefore_timestampset_period(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	bool result = overbefore_timestampset_period_internal(ts, p);
	PG_FREE_IF_COPY(ts, 0);
	PG_RETURN_BOOL(result);
}

bool
overbefore_timestampset_periodset_internal(TimestampSet *ts, PeriodSet *ps)
{
	TimestampTz t = timestampset_time_n(ts, ts->count-1);
	Period *p = periodset_per_n(ps, ps->count-1);
	return (!after_timestamp_period_internal(t, p));
}

PG_FUNCTION_INFO_V1(overbefore_timestampset_periodset);

PGDLLEXPORT Datum
overbefore_timestampset_periodset(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = overbefore_timestampset_periodset_internal(ts, ps);
	PG_FREE_IF_COPY(ts, 0);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}

bool
overbefore_period_timestamp_internal(Period *p, TimestampTz t)
{
	return (period_cmp_bounds(p->upper, t, false, false, 
			p->upper_inc, true) <= 0);
}

PG_FUNCTION_INFO_V1(overbefore_period_timestamp);

PGDLLEXPORT Datum
overbefore_period_timestamp(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	PG_RETURN_BOOL(overbefore_period_timestamp_internal(p, t));
}

bool
overbefore_period_timestampset_internal(Period *p, TimestampSet *ts)
{
	TimestampTz t = timestampset_time_n(ts, ts->count-1);
	return (overbefore_period_timestamp_internal(p, t));
}

PG_FUNCTION_INFO_V1(overbefore_period_timestampset);

PGDLLEXPORT Datum
overbefore_period_timestampset(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = overbefore_period_timestampset_internal(p, ts);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

bool
overbefore_period_period_internal(Period *p1, Period *p2)
{
	return (period_cmp_bounds(p1->upper, p2->upper, false, false, 
			p1->upper_inc, p2->upper_inc) <= 0);
}

PG_FUNCTION_INFO_V1(overbefore_period_period);

PGDLLEXPORT Datum
overbefore_period_period(PG_FUNCTION_ARGS)
{
	Period *p1 = PG_GETARG_PERIOD(0);
	Period *p2 = PG_GETARG_PERIOD(1);
	PG_RETURN_BOOL(overbefore_period_period_internal(p1, p2));
}

bool
overbefore_period_periodset_internal(Period *p, PeriodSet *ps)
{
	Period *p1 = periodset_per_n(ps, ps->count-1);
	return (period_cmp_bounds(p->upper, p1->upper, false, false, 
			p->upper_inc, p1->upper_inc) <= 0);
}

PG_FUNCTION_INFO_V1(overbefore_period_periodset);

PGDLLEXPORT Datum
overbefore_period_periodset(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = overbefore_period_periodset_internal(p, ps);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}


bool
overbefore_periodset_timestamp_internal(PeriodSet *ps, TimestampTz t)
{
	Period *p = periodset_per_n(ps, ps->count-1);
	return (period_cmp_bounds(p->upper, t, false, false, 
			p->upper_inc, true) <= 0);
}

PG_FUNCTION_INFO_V1(overbefore_periodset_timestamp);

PGDLLEXPORT Datum
overbefore_periodset_timestamp(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	bool result = overbefore_periodset_timestamp_internal(ps, t);
	PG_FREE_IF_COPY(ps, 0);
	PG_RETURN_BOOL(result);
}

bool
overbefore_periodset_timestampset_internal(PeriodSet *ps, TimestampSet *ts)
{
	TimestampTz t1 = periodset_end_timestamp_internal(ps);
	TimestampTz t2 = timestampset_time_n(ts, ts->count-1);
	return (timestamp_cmp_internal(t1, t2) <= 0);
}

PG_FUNCTION_INFO_V1(overbefore_periodset_timestampset);

PGDLLEXPORT Datum
overbefore_periodset_timestampset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = overbefore_periodset_timestampset_internal(ps, ts);
	PG_FREE_IF_COPY(ps, 0);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

bool
overbefore_periodset_period_internal(PeriodSet *ps, Period *p)
{
	Period *p1 = periodset_per_n(ps, ps->count-1);
	return (period_cmp_bounds(p1->upper, p->upper, false, false, 
			p1->upper_inc, p->upper_inc) <= 0);
}

PG_FUNCTION_INFO_V1(overbefore_periodset_period);

PGDLLEXPORT Datum
overbefore_periodset_period(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	bool result = overbefore_periodset_period_internal(ps, p);
	PG_FREE_IF_COPY(ps, 0);
	PG_RETURN_BOOL(result);
}

bool
overbefore_periodset_periodset_internal(PeriodSet *ps1, PeriodSet *ps2)
{
	Period *p1 = periodset_per_n(ps1, ps1->count-1);
	Period *p2 = periodset_per_n(ps2, ps2->count-1);
	return (period_cmp_bounds(p1->upper, p2->upper, false, false, 
			p1->upper_inc, p2->upper_inc) <= 0);
}

PG_FUNCTION_INFO_V1(overbefore_periodset_periodset);

PGDLLEXPORT Datum
overbefore_periodset_periodset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps1 = PG_GETARG_PERIODSET(0);
	PeriodSet *ps2 = PG_GETARG_PERIODSET(1);
	bool result = overbefore_periodset_periodset_internal(ps1, ps2);
	PG_FREE_IF_COPY(ps1, 0);
	PG_FREE_IF_COPY(ps2, 1);
	PG_RETURN_BOOL(result);
}

/*****************************************************************************/
/* does not extend to left of? */

bool
overafter_timestamp_timestampset_internal(TimestampTz t, TimestampSet *ts)
{
	TimestampTz t1 = timestampset_time_n(ts, 0);
	return (timestamp_cmp_internal(t, t1) >= 0);
}

PG_FUNCTION_INFO_V1(overafter_timestamp_timestampset);

PGDLLEXPORT Datum
overafter_timestamp_timestampset(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = overafter_timestamp_timestampset_internal(t, ts);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

bool
overafter_timestamp_period_internal(TimestampTz t, Period *p)
{
	return (period_cmp_bounds(t, p->lower, true, true, 
			true, p->lower_inc) >= 0);
}

PG_FUNCTION_INFO_V1(overafter_timestamp_period);

PGDLLEXPORT Datum
overafter_timestamp_period(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	Period *p = PG_GETARG_PERIOD(1);
	PG_RETURN_BOOL(overafter_timestamp_period_internal(t, p));
}

bool
overafter_timestamp_periodset_internal(TimestampTz t, PeriodSet *ps)
{
	Period *p = periodset_per_n(ps, 0);
	return (period_cmp_bounds(t, p->lower, true, true, 
			true, p->lower_inc) >= 0);
}

PG_FUNCTION_INFO_V1(overafter_timestamp_periodset);

PGDLLEXPORT Datum
overafter_timestamp_periodset(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = overafter_timestamp_periodset_internal(t, ps);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}

bool
overafter_timestampset_timestamp_internal(TimestampSet *ts, TimestampTz t)
{
	TimestampTz t1 = timestampset_time_n(ts, 0);
	return (timestamp_cmp_internal(t1, t) >= 0);
}

PG_FUNCTION_INFO_V1(overafter_timestampset_timestamp);

PGDLLEXPORT Datum
overafter_timestampset_timestamp(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	bool result = overafter_timestampset_timestamp_internal(ts, t);
	PG_FREE_IF_COPY(ts, 0);
	PG_RETURN_BOOL(result);
}

bool
overafter_timestampset_timestampset_internal(TimestampSet *ts1, TimestampSet *ts2)
{
	TimestampTz t1 = timestampset_time_n(ts1, 0);
	TimestampTz t2 = timestampset_time_n(ts2, 0);
	return (timestamp_cmp_internal(t1, t2) >= 0);
}

PG_FUNCTION_INFO_V1(overafter_timestampset_timestampset);

PGDLLEXPORT Datum
overafter_timestampset_timestampset(PG_FUNCTION_ARGS)
{
	TimestampSet *ts1 = PG_GETARG_TIMESTAMPSET(0);
	TimestampSet *ts2 = PG_GETARG_TIMESTAMPSET(1);
	bool result = overafter_timestampset_timestampset_internal(ts1, ts2);
	PG_FREE_IF_COPY(ts1, 0);
	PG_FREE_IF_COPY(ts2, 1);
	PG_RETURN_BOOL(result);
}

bool
overafter_timestampset_period_internal(TimestampSet *ts, Period *p)
{
	TimestampTz t = timestampset_time_n(ts, 0);
	return (overafter_timestamp_period_internal(t, p));
}

PG_FUNCTION_INFO_V1(overafter_timestampset_period);

PGDLLEXPORT Datum
overafter_timestampset_period(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	bool result = overafter_timestampset_period_internal(ts, p);
	PG_FREE_IF_COPY(ts, 0);
	PG_RETURN_BOOL(result);
}

bool
overafter_timestampset_periodset_internal(TimestampSet *ts, PeriodSet *ps)
{
	TimestampTz t = timestampset_time_n(ts, 0);
	Period *p = periodset_per_n(ps, 0);
	return (overafter_timestamp_period_internal(t, p));
}

PG_FUNCTION_INFO_V1(overafter_timestampset_periodset);

PGDLLEXPORT Datum
overafter_timestampset_periodset(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = overafter_timestampset_periodset_internal(ts, ps);
	PG_FREE_IF_COPY(ts, 0);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}

bool
overafter_period_timestamp_internal(Period *p, TimestampTz t)
{
	return (period_cmp_bounds(p->lower, t, true, true, 
			p->lower_inc, true) >= 0);
}

PG_FUNCTION_INFO_V1(overafter_period_timestamp);

PGDLLEXPORT Datum
overafter_period_timestamp(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	PG_RETURN_BOOL(overafter_period_timestamp_internal(p, t));
}

bool
overafter_period_timestampset_internal(Period *p, TimestampSet *ts)
{
	TimestampTz t = timestampset_time_n(ts, 0);
	return (overafter_period_timestamp_internal(p, t));
}

PG_FUNCTION_INFO_V1(overafter_period_timestampset);

PGDLLEXPORT Datum
overafter_period_timestampset(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = overafter_period_timestampset_internal(p, ts);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

bool
overafter_period_period_internal(Period *p1, Period *p2)
{
	return (period_cmp_bounds(p1->lower, p2->lower, true, true, 
			p1->lower_inc, p2->lower_inc) >= 0);
}

PG_FUNCTION_INFO_V1(overafter_period_period);

PGDLLEXPORT Datum
overafter_period_period(PG_FUNCTION_ARGS)
{
	Period *p1 = PG_GETARG_PERIOD(0);
	Period *p2 = PG_GETARG_PERIOD(1);
	PG_RETURN_BOOL(overafter_period_period_internal(p1, p2));
}

bool
overafter_period_periodset_internal(Period *p, PeriodSet *ps)
{
	Period *p1 = periodset_per_n(ps, 0);
	return (period_cmp_bounds(p->lower, p1->lower, true, true, 
			p->lower_inc, p1->lower_inc) >= 0);
}

PG_FUNCTION_INFO_V1(overafter_period_periodset);

PGDLLEXPORT Datum
overafter_period_periodset(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = overafter_period_periodset_internal(p, ps);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}

bool
overafter_periodset_timestamp_internal(PeriodSet *ps, TimestampTz t)
{
	Period *p = periodset_per_n(ps, 0);
	return (period_cmp_bounds(p->lower, t, true, true, 
			p->lower_inc, true) >= 0);
}

PG_FUNCTION_INFO_V1(overafter_periodset_timestamp);

PGDLLEXPORT Datum
overafter_periodset_timestamp(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	bool result = overafter_periodset_timestamp_internal(ps, t);
	PG_FREE_IF_COPY(ps, 0);
	PG_RETURN_BOOL(result);
}

bool
overafter_periodset_timestampset_internal(PeriodSet *ps, TimestampSet *ts)
{
	TimestampTz t1 = periodset_start_timestamp_internal(ps);
	TimestampTz t2 = timestampset_time_n(ts, 0);
	return (timestamp_cmp_internal(t1, t2) >= 0);
}

PG_FUNCTION_INFO_V1(overafter_periodset_timestampset);

PGDLLEXPORT Datum
overafter_periodset_timestampset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = overafter_periodset_timestampset_internal(ps, ts);
	PG_FREE_IF_COPY(ps, 0);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

bool
overafter_periodset_period_internal(PeriodSet *ps, Period *p)
{
	Period *p1 = periodset_per_n(ps, 0);
	return (period_cmp_bounds(p1->lower, p->lower, true, true, 
			p1->lower_inc, p->lower_inc) >= 0);
}

PG_FUNCTION_INFO_V1(overafter_periodset_period);

PGDLLEXPORT Datum
overafter_periodset_period(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	bool result = overafter_periodset_period_internal(ps, p);
	PG_FREE_IF_COPY(ps, 0);
	PG_RETURN_BOOL(result);
}

bool
overafter_periodset_periodset_internal(PeriodSet *ps1, PeriodSet *ps2)
{
	Period *p1 = periodset_per_n(ps1, 0);
	Period *p2 = periodset_per_n(ps2, 0);
	return (period_cmp_bounds(p1->lower, p2->lower, true, true, 
			p1->lower_inc, p2->lower_inc) >= 0);
}

PG_FUNCTION_INFO_V1(overafter_periodset_periodset);

PGDLLEXPORT Datum
overafter_periodset_periodset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps1 = PG_GETARG_PERIODSET(0);
	PeriodSet *ps2 = PG_GETARG_PERIODSET(1);
	bool result = overafter_periodset_periodset_internal(ps1, ps2);
	PG_FREE_IF_COPY(ps1, 0);
	PG_FREE_IF_COPY(ps2, 1);
	PG_RETURN_BOOL(result);
}

/*****************************************************************************/
/* adjacent to (but not overlapping)? */

bool
adjacent_timestamp_period_internal(TimestampTz t, Period *p)
{
	/*
	 * Two periods A..B and C..D are adjacent if and only if
	 * B is adjacent to C, or D is adjacent to A.
	 */
	return (period_bounds_adjacent(t, p->lower, 
				true, p->lower_inc) ||
			period_bounds_adjacent(p->upper, t, 
				p->upper_inc, true));
}

PG_FUNCTION_INFO_V1(adjacent_timestamp_period);

PGDLLEXPORT Datum
adjacent_timestamp_period(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	Period *p = PG_GETARG_PERIOD(1);
	PG_RETURN_BOOL(adjacent_timestamp_period_internal(t, p));
}

bool
adjacent_timestamp_periodset_internal(TimestampTz t, PeriodSet *ps)
{
	/*
	 * Two periods A..B and C..D are adjacent if and only if
	 * B is adjacent to C, or D is adjacent to A.
	 */
	Period *p1 = periodset_per_n(ps, 0);
	Period *p2 = periodset_per_n(ps, ps->count-1);
	return (period_bounds_adjacent(t, p1->lower, 
				true, p1->lower_inc) ||
			period_bounds_adjacent(p2->upper, t, 
				p2->upper_inc, true));
}

PG_FUNCTION_INFO_V1(adjacent_timestamp_periodset);

PGDLLEXPORT Datum
adjacent_timestamp_periodset(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = adjacent_timestamp_periodset_internal(t, ps);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}

bool
adjacent_timestampset_period_internal(TimestampSet *ts, Period *p)
{
	/*
	 * A periods A..B and a timestamptz C are adjacent if and only if
	 * B is adjacent to C, or C is adjacent to A.
	 */
	TimestampTz t1 = timestampset_time_n(ts, 0);
	TimestampTz t2 = timestampset_time_n(ts, ts->count-1);
	return (period_bounds_adjacent(p->upper, t1,
				p->upper_inc, true) ||
			period_bounds_adjacent(t2, p->lower, 
				true, p->lower_inc));
}

PG_FUNCTION_INFO_V1(adjacent_timestampset_period);

PGDLLEXPORT Datum
adjacent_timestampset_period(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	bool result = adjacent_timestampset_period_internal(ts, p);
	PG_FREE_IF_COPY(ts, 0);
	PG_RETURN_BOOL(result);
}

bool
adjacent_timestampset_periodset_internal(TimestampSet *ts, PeriodSet *ps)
{
	/*
	 * A periods A..B and a timestamptz C are adjacent if and only if
	 * B is adjacent to C, or C is adjacent to A.
	 */
	TimestampTz t1 = timestampset_time_n(ts, 0);
	TimestampTz t2 = timestampset_time_n(ts, ts->count-1);
	Period *p1 = periodset_per_n(ps, 0);
	Period *p2 = periodset_per_n(ps, ps->count-1);
	return (period_bounds_adjacent(p2->upper, t1,
				p2->upper_inc, true) ||
			period_bounds_adjacent(t2, p1->lower, 
				true, p2->lower_inc));
}

PG_FUNCTION_INFO_V1(adjacent_timestampset_periodset);

PGDLLEXPORT Datum
adjacent_timestampset_periodset(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = adjacent_timestampset_periodset_internal(ts, ps);
	PG_FREE_IF_COPY(ts, 0);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}

bool
adjacent_period_timestamp_internal(Period *p, TimestampTz t)
{
	/*
	 * A periods A..B and a timestamptz C are adjacent if and only if
	 * B is adjacent to C, or C is adjacent to A.
	 */
	return (period_bounds_adjacent(p->upper, t, 
				p->upper_inc, true) ||
			period_bounds_adjacent(t, p->lower, 
				true, p->lower_inc));
}

PG_FUNCTION_INFO_V1(adjacent_period_timestamp);

PGDLLEXPORT Datum
adjacent_period_timestamp(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	PG_RETURN_BOOL(adjacent_period_timestamp_internal(p, t));
}

bool
adjacent_period_timestampset_internal(Period *p, TimestampSet *ts)
{
	/*
	 * A periods A..B and a timestamptz C are adjacent if and only if
	 * B is adjacent to C, or C is adjacent to A.
	 */
	TimestampTz t1 = timestampset_time_n(ts, 0);
	TimestampTz t2 = timestampset_time_n(ts, ts->count-1);
	return (period_bounds_adjacent(p->upper, t1,
				p->upper_inc, true) ||
			period_bounds_adjacent(t2, p->lower, 
				true, p->lower_inc));
}

PG_FUNCTION_INFO_V1(adjacent_period_timestampset);

PGDLLEXPORT Datum
adjacent_period_timestampset(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = adjacent_period_timestampset_internal(p, ts);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

bool
adjacent_period_period_internal(Period *p1, Period *p2)
{
	/*
	 * Two periods A..B and C..D are adjacent if and only if
	 * B is adjacent to C, or D is adjacent to A.
	 */
	return (period_bounds_adjacent(p1->upper, p2->lower, 
				p1->upper_inc, p2->lower_inc) ||
			period_bounds_adjacent(p2->upper, p1->lower, 
				p2->upper_inc, p1->lower_inc));
}

PG_FUNCTION_INFO_V1(adjacent_period_period);

PGDLLEXPORT Datum
adjacent_period_period(PG_FUNCTION_ARGS)
{
	Period *p1 = PG_GETARG_PERIOD(0);
	Period *p2 = PG_GETARG_PERIOD(1);
	PG_RETURN_BOOL(adjacent_period_period_internal(p1, p2));
}

bool
adjacent_period_periodset_internal(Period *p, PeriodSet *ps)
{
	Period *p1 = periodset_per_n(ps, 0);
	Period *p2 = periodset_per_n(ps, ps->count-1);
	/*
	 * Two periods A..B and C..D are adjacent if and only if
	 * B is adjacent to C, or D is adjacent to A.
	 */
	return (period_bounds_adjacent(p2->upper, p->lower, 
				p2->upper_inc, p->lower_inc) ||
			period_bounds_adjacent(p->upper, p1->lower, 
				p->upper_inc, p1->lower_inc));
}

PG_FUNCTION_INFO_V1(adjacent_period_periodset);

PGDLLEXPORT Datum
adjacent_period_periodset(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool result = adjacent_period_periodset_internal(p, ps);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_BOOL(result);
}

bool
adjacent_periodset_timestamp_internal(PeriodSet *ps, TimestampTz t)
{
	/*
	 * A periods A..B and a timestamptz C are adjacent if and only if
	 * B is adjacent to C, or C is adjacent to A.
	 */
	Period *p1 = periodset_per_n(ps, 0);
	Period *p2 = periodset_per_n(ps, ps->count-1);
	return (period_bounds_adjacent(t, p1->lower, 
				true, p1->lower_inc) ||
			period_bounds_adjacent(p2->upper, t, 
				p2->upper_inc, true));
}

PG_FUNCTION_INFO_V1(adjacent_periodset_timestamp);

PGDLLEXPORT Datum
adjacent_periodset_timestamp(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	bool result = adjacent_periodset_timestamp_internal(ps, t);
	PG_FREE_IF_COPY(ps, 0);
	PG_RETURN_BOOL(result);
}

bool
adjacent_periodset_timestampset_internal(PeriodSet *ps, TimestampSet *ts)
{
	/*
	 * A periods A..B and a timestamptz C are adjacent if and only if
	 * B is adjacent to C, or C is adjacent to A.
	 */
	Period *p1 = periodset_per_n(ps, 0);
	Period *p2 = periodset_per_n(ps, ps->count-1);
	TimestampTz t1 = timestampset_time_n(ts, 0);
	TimestampTz t2 = timestampset_time_n(ts, ts->count-1);
	return (period_bounds_adjacent(t2, p1->lower, 
				true, p1->lower_inc) ||
			period_bounds_adjacent(p2->upper, t1, 
				p2->upper_inc, true));
}

PG_FUNCTION_INFO_V1(adjacent_periodset_timestampset);

PGDLLEXPORT Datum
adjacent_periodset_timestampset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool result = adjacent_periodset_timestampset_internal(ps, ts);
	PG_FREE_IF_COPY(ps, 0);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_BOOL(result);
}

bool
adjacent_periodset_period_internal(PeriodSet *ps, Period *p)
{
	Period *p1 = periodset_per_n(ps, 0);
	Period *p2 = periodset_per_n(ps, ps->count-1);
	/*
	 * Two periods A..B and C..D are adjacent if and only if
	 * B is adjacent to C, or D is adjacent to A.
	 */
	return (period_bounds_adjacent(p->upper, p1->lower, 
				p->upper_inc, p1->lower_inc) ||
			period_bounds_adjacent(p2->upper, p->lower, 
				p2->upper_inc, p->lower_inc));
}

PG_FUNCTION_INFO_V1(adjacent_periodset_period);

PGDLLEXPORT Datum
adjacent_periodset_period(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	bool result = adjacent_periodset_period_internal(ps, p);
	PG_FREE_IF_COPY(ps, 0);
	PG_RETURN_BOOL(result);
}

bool
adjacent_periodset_periodset_internal(PeriodSet *ps1, PeriodSet *ps2)
{
	Period *startps1 = periodset_per_n(ps1, 0);
	Period *endps1 = periodset_per_n(ps1, ps1->count-1);
	Period *startps2 = periodset_per_n(ps2, 0);
	Period *endps2 = periodset_per_n(ps2, ps2->count-1);
	/*
	 * Two periods A..B and C..D are adjacent if and only if
	 * B is adjacent to C, or D is adjacent to A.
	 */
	return (period_bounds_adjacent(endps1->upper, startps2->lower, 
				endps1->upper_inc, startps2->lower_inc) ||
			period_bounds_adjacent(endps2->upper, startps1->lower, 
				endps2->upper_inc, startps1->lower_inc));
}

PG_FUNCTION_INFO_V1(adjacent_periodset_periodset);

PGDLLEXPORT Datum
adjacent_periodset_periodset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps1 = PG_GETARG_PERIODSET(0);
	PeriodSet *ps2 = PG_GETARG_PERIODSET(1);
	bool result = adjacent_periodset_periodset_internal(ps1, ps2);
	PG_FREE_IF_COPY(ps1, 0);
	PG_FREE_IF_COPY(ps2, 1);
	PG_RETURN_BOOL(result);
}

/*****************************************************************************
 * Set union 
 *****************************************************************************/

TimestampSet *
union_timestamp_timestampset_internal(TimestampTz t, TimestampSet *ts)
{
	TimestampTz *times = palloc(sizeof(Timestamp) * (ts->count + 1));
	int k = 0;
	bool found = false;
	for (int i = 0; i < ts->count; i++)
	{
		TimestampTz t1 = timestampset_time_n(ts, i);
		if (!found)
		{
			if (timestamp_cmp_internal(t, t1) < 0)
			{
				times[k++] = t;
				found = true;
			}
			if (timestamp_cmp_internal(t, t1) == 0)
				found = true;
		}
		times[k++] = t1;
	}
	if (!found)
		times[k++] = t;
	TimestampSet *result = timestampset_from_timestamparr_internal(times, k);
	pfree(times);
	return result;		
}

PG_FUNCTION_INFO_V1(union_timestamp_timestampset);

PGDLLEXPORT Datum
union_timestamp_timestampset(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	TimestampSet *result = union_timestamp_timestampset_internal(t, ts);
	PG_FREE_IF_COPY(ts, 1);
	PG_RETURN_POINTER(result);
}

/*****************************************************************************/

PG_FUNCTION_INFO_V1(union_timestampset_timestamp);

PGDLLEXPORT Datum
union_timestampset_timestamp(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	TimestampSet *result = union_timestamp_timestampset_internal(t, ts);
	PG_FREE_IF_COPY(ts, 0);
	PG_RETURN_POINTER(result);
}

TimestampSet *
union_timestampset_timestampset_internal(TimestampSet *ts1, TimestampSet *ts2)
{
	TimestampTz *times = palloc(sizeof(Timestamp) * (ts1->count + ts2->count));
	TimestampTz t1 = timestampset_time_n(ts1, 0);
	TimestampTz t2 = timestampset_time_n(ts2, 0);
	int i = 0, j = 0, k = 0;
	while (i < ts1->count && j < ts2->count)
	{
		if (timestamp_cmp_internal(t1, t2) == 0)
		{
			times[k++] = t1;
			t1 = timestampset_time_n(ts1, ++i);
			t2 = timestampset_time_n(ts2, ++j);
		}
		else if (timestamp_cmp_internal(t1, t2) < 0)
		{
			times[k++] = t1;
			t1 = timestampset_time_n(ts1, ++i);
		}
		else
		{
			times[k++] = t2;
			t2 = timestampset_time_n(ts2, ++j);
		}		
	}
	while (i < ts1->count)
		times[k++] = timestampset_time_n(ts1, i++);
	while (j < ts2->count)
		times[k++] = timestampset_time_n(ts2, j++);

	TimestampSet *result = timestampset_from_timestamparr_internal(times, k);
	pfree(times);
	return result;		
}

PG_FUNCTION_INFO_V1(union_timestampset_timestampset);

PGDLLEXPORT Datum
union_timestampset_timestampset(PG_FUNCTION_ARGS)
{
	TimestampSet *ts1 = PG_GETARG_TIMESTAMPSET(0);
	TimestampSet *ts2 = PG_GETARG_TIMESTAMPSET(1);
	TimestampSet *result = union_timestampset_timestampset_internal(ts1, ts2);
	PG_FREE_IF_COPY(ts1, 0);
	PG_FREE_IF_COPY(ts2, 1);
	PG_RETURN_POINTER(result);
}

/*****************************************************************************/

PeriodSet *
union_period_period_internal(Period *p1, Period *p2)
{
	TimestampTz result_lower;
	TimestampTz result_upper;
	bool result_lower_inc;
	bool result_upper_inc;

	if (!overlaps_period_period_internal(p1, p2) &&
		!adjacent_period_period_internal(p1, p2))
	{
		Period **periods = palloc(sizeof(Period) * 2);
		if (timestamp_cmp_internal(p1->lower, p2->lower) < 0)
		{
			periods[0] = p1;
			periods[1] = p2;
		}
		else
		{
			periods[0] = p2;
			periods[1] = p1;			
		}
		PeriodSet *result = periodset_from_periodarr_internal(periods, 2, false);
		pfree(periods);
		return result;		
	}

	if (period_cmp_bounds(p1->lower, p2->lower, true, true, 
		p1->lower_inc, p2->lower_inc) < 0)
	{
		result_lower = p1->lower;
		result_lower_inc = p1->lower_inc;
	}
	else
	{
		result_lower = p2->lower;
		result_lower_inc = p2->lower_inc;
	}

	if (period_cmp_bounds(p1->upper, p2->upper, false, false, 
		p1->upper_inc, p2->upper_inc) > 0)
	{
		result_upper = p1->upper;
		result_upper_inc = p1->upper_inc;
	}
	else
	{
		result_upper = p2->upper;
		result_upper_inc = p2->upper_inc;
	}

	Period *p = period_make(result_lower, result_upper, 
		result_lower_inc, result_upper_inc);
	PeriodSet *result = periodset_from_periodarr_internal(&p, 1, false);
	pfree(p);
	return result;
}

PG_FUNCTION_INFO_V1(union_period_period);

PGDLLEXPORT Datum
union_period_period(PG_FUNCTION_ARGS)
{
	Period *p1 = PG_GETARG_PERIOD(0);
	Period *p2 = PG_GETARG_PERIOD(1);
	PG_RETURN_POINTER(union_period_period_internal(p1, p2));
}

PeriodSet *
union_period_periodset_internal(Period *p, PeriodSet *ps)
{
	/* If the period set does not overlap with the period */
	if (!overlaps_period_periodset_internal(p, ps))
	{
		Period **periods = palloc(sizeof(Period *) * (ps->count + 1));
		int k = 0;
		bool found = false;
		for (int i = 0; i < ps->count; i++)
		{
			Period *p1 = periodset_per_n(ps, i);
			if (!found && before_period_period_internal(p, p1))
			{
				periods[k++] = p;
				found = true;
			}
			periods[k++] = p1;
		}
		PeriodSet *result = periodset_from_periodarr_internal(periods, k, true);
		pfree(periods);
		return result;		
	}
	
	/* If the period set is contained in the period */
	if (contained_periodset_period_internal(ps, p))
		return periodset_from_periodarr_internal(&p, 1, false);
	
	/* Binary search of lower and upper bounds of period */
	int n1 = periodset_find_timestamp(ps, p->lower);
	int n2 = periodset_find_timestamp(ps, p->upper);

	/* If lower and/or upper are before/after the period set or in a gap, 
	   find n1 and/or n2 which are, respectively, the first/last overlapping 
	   periods 
		        |-----------------|
		|------| |------| |------| |------| 
	*/
	Period *p1, *p2, p3;
	if (n1 == -1)
	{
		n1 = 0;
		p1 = periodset_per_n(ps, 0);
		while (!overlaps_period_period_internal(p, p1))
			p1 = periodset_per_n(ps, ++n1);
	}
	else
		p1 = periodset_per_n(ps, n1);
	if (n2 == -1)
	{
		n2 = ps->count-1;
		p2 = periodset_per_n(ps, ps->count-1);
		while (!overlaps_period_period_internal(p, p2))
			p2 = periodset_per_n(ps, --n2);
	}
	else
		p2 = periodset_per_n(ps, n2);
		
	/* Compute the union of the period with the overlapping periods */
	TimestampTz lower, upper; 
	bool lower_inc, upper_inc; 
	if (period_cmp_bounds(p->lower, p1->lower, true, true, 
		p->lower_inc, p1->lower_inc) < 0)
	{
		lower = p->lower;
		lower_inc = p->lower_inc;
	}
	else
	{
		lower = p1->lower;
		lower_inc = p1->lower_inc;
	}
	if (period_cmp_bounds(p->upper, p1->upper, false, false, 
		p->upper_inc, p1->upper_inc) > 0)
	{
		upper = p->upper;
		upper_inc = p->lower_inc;
	}
	else
	{
		upper = p1->upper;
		upper_inc = p1->upper_inc;
	}
	period_set(&p3, lower, upper, lower_inc, upper_inc);
	
	Period **periods = palloc(sizeof(Period *) * (ps->count - n2 + n1));
	/* Copy the first n1-1 periods */
	int k = 0;
	for (int i = 0; i < n1; i++)
		periods[k++] = periodset_per_n(ps, i);
	periods[k++] = &p3;
	/* Copy the last n2+1 periods */
	for (int i = n2+1; i < ps->count; i++)
		periods[k++] = periodset_per_n(ps, i);
	PeriodSet *result = periodset_from_periodarr_internal(periods, k, true);
	pfree(periods);
	return result;
}

PG_FUNCTION_INFO_V1(union_period_periodset);

PGDLLEXPORT Datum
union_period_periodset(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	PeriodSet *result = union_period_periodset_internal(p, ps);
	PG_FREE_IF_COPY(ps, 1);
	PG_RETURN_POINTER(result);
}

/*****************************************************************************/

PG_FUNCTION_INFO_V1(union_periodset_period);

PGDLLEXPORT Datum
union_periodset_period(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	PeriodSet *result = union_period_periodset_internal(p, ps);
	PG_FREE_IF_COPY(ps, 0);
	PG_RETURN_POINTER(result);
}

PeriodSet *
union_periodset_periodset_internal(PeriodSet *ps1, PeriodSet *ps2)
{
	/* If the period sets do not overlap */
	if (!overlaps_periodset_periodset_internal(ps1, ps2))
	{
		Period **periods = palloc(sizeof(Period *) * (ps1->count + ps2->count));
		int i = 0, j = 0, k = 0;
		while (i < ps1->count && j < ps2->count)
		{
			Period *p1 = periodset_per_n(ps1, i);
			Period *p2 = periodset_per_n(ps2, j);
			if (before_period_period_internal(p1, p2))
			{
				periods[k++] = p1;
				i++;
			}
			else
			{
				periods[k++] = p2;
				j++;
			}		
		}
		PeriodSet *result = periodset_from_periodarr_internal(periods, k, false);
		pfree(periods);
		return result;		
	}

	Period **periods = palloc(sizeof(Period *) * (ps1->count + ps2->count));
	Period **mustfree = palloc(sizeof(Period *) * Max(ps1->count, ps2->count));
	int i = 0, j = 0, k = 0, l = 0;
	while (i < ps1->count && j < ps2->count)
	{
		Period *p1 = periodset_per_n(ps1, i);
		Period *p2 = periodset_per_n(ps2, j);
		/* The periods do not overlap, copy the earliest period */
		if (!overlaps_period_period_internal(p1, p2))
		{
			if (before_period_period_internal(p1, p2))
			{
				periods[k++] = p1;
				i++;
			}
			else
			{
				periods[k++] = p2;
				j++;
			}
		}
		else
		{
			/* Find all periods in ps1 that overlap with periods in ps2 
				   i                 i
				|-----|  |-----|  |-----|  |-----|      
				     |-----|  |-----|  
					    j        j
			*/
			Period *q1 = NULL, *q2 = NULL; /* keep compiler quiet */
			/* remember whether i or j was the last value incremented */
			bool ilastinc = false, jlastinc = false;
			while (i < ps1->count && j < ps2->count)
			{
				q1 = periodset_per_n(ps1, i);
				q2 = periodset_per_n(ps2, j);
				if (overlaps_period_period_internal(q1, q2))
				{
					if (timestamp_cmp_internal(q1->upper, q2->upper) == 0)
					{
						i++; j++;			
						ilastinc = true; jlastinc = true;

					}
					else if (timestamp_cmp_internal(q1->upper, q2->upper) < 0)
					{
						i++;
						ilastinc = true; jlastinc = false;
					}
					else
					{
						j++;
						ilastinc = false; jlastinc = true;
					}
				}
				else
					break;
			}
			/* Put after the value of last counter to be incremented */
			if (ilastinc)
				q1 = periodset_per_n(ps1, --i);
			if (jlastinc)
				q2 = periodset_per_n(ps2, --j);
			/* Compute the union of the overlapping periods */
			TimestampTz lower, upper; 
			bool lower_inc, upper_inc; 
			if (period_cmp_bounds(p1->lower, p2->lower, true, true, 
				p1->lower_inc, p2->lower_inc) < 0)
			{
				lower = p1->lower;
				lower_inc = p1->lower_inc;
			}
			else
			{
				lower = p2->lower;
				lower_inc = p2->lower_inc;
			}
			if (period_cmp_bounds(q1->upper, q2->upper, false, false, 
				q1->upper_inc, q2->upper_inc) > 0)
			{
				upper = q1->upper;
				upper_inc = q1->upper_inc;
			}
			else
			{
				upper = q2->upper;
				upper_inc = q2->upper_inc;
			}
			Period *p3 = period_make(lower, upper, lower_inc, upper_inc);
			periods[k++] = p3;
			mustfree[l++] = p3;
			i++; j++;			
		}
	}
	while (i < ps1->count)
		periods[k++] = periodset_per_n(ps1, i++);
	while (j < ps2->count)
		periods[k++] = periodset_per_n(ps2, j++);

	if (k == 0)
	{
		pfree(periods);
		return NULL;
	}
	
	PeriodSet *result = periodset_from_periodarr_internal(periods, k, true);	

	pfree(periods);
	for (int i = 0; i < l; i++)
		pfree(mustfree[i]);
	pfree(mustfree);

	return result;
}

PG_FUNCTION_INFO_V1(union_periodset_periodset);

PGDLLEXPORT Datum
union_periodset_periodset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps1 = PG_GETARG_PERIODSET(0);
	PeriodSet *ps2 = PG_GETARG_PERIODSET(1);
	PeriodSet *result = union_periodset_periodset_internal(ps1, ps2);
	PG_FREE_IF_COPY(ps1, 0);
	PG_FREE_IF_COPY(ps2, 1);
	PG_RETURN_POINTER(result);
}

/*****************************************************************************
 * Set intersection 
 *****************************************************************************/

PG_FUNCTION_INFO_V1(intersection_timestamp_timestampset);

PGDLLEXPORT Datum
intersection_timestamp_timestampset(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool contains = contains_timestampset_timestamp_internal(ts, t);
	PG_FREE_IF_COPY(ts, 1);
	if (!contains)
		PG_RETURN_NULL();
	PG_RETURN_TIMESTAMPTZ(t);
}

PG_FUNCTION_INFO_V1(intersection_timestampset_timestamp);

PGDLLEXPORT Datum
intersection_timestampset_timestamp(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	bool contains = contains_timestampset_timestamp_internal(ts, t);
	PG_FREE_IF_COPY(ts, 0);
	if (!contains)
		PG_RETURN_NULL();
	PG_RETURN_TIMESTAMPTZ(t);
}

TimestampSet *
intersection_timestampset_timestampset_internal(TimestampSet *ts1, TimestampSet *ts2)
{
	/* Test whether the timespan of the two timestamp sets overlap */
	Period *p1 = timestampset_bbox(ts1);
	Period *p2 = timestampset_bbox(ts2);
	if (!overlaps_period_period_internal(p1, p2))
		return NULL;

	TimestampTz *times = palloc(sizeof(Timestamp) * (ts1->count + ts2->count));
	int i = 0, j = 0, k = 0;
	while (i < ts1->count && j < ts2->count)
	{
		TimestampTz t1 = timestampset_time_n(ts1, i);
		TimestampTz t2 = timestampset_time_n(ts2, j);
		if (timestamp_cmp_internal(t1, t2) == 0)
		{
			times[k++] = t1;
			i++; j++;
		}
		else if (timestamp_cmp_internal(t1, t2) < 0)
			i++;
		else 
			j++;
	}
	if (k == 0)
	{
		pfree(times);
		return NULL;
	}
	
	TimestampSet *result = timestampset_from_timestamparr_internal(times, k);	

	pfree(times);

	return result;
}

PG_FUNCTION_INFO_V1(intersection_timestampset_timestampset);

PGDLLEXPORT Datum
intersection_timestampset_timestampset(PG_FUNCTION_ARGS)
{
	TimestampSet *ts1 = PG_GETARG_TIMESTAMPSET(0);
	TimestampSet *ts2 = PG_GETARG_TIMESTAMPSET(1);
	TimestampSet *result = intersection_timestampset_timestampset_internal(ts1, ts2);
	PG_FREE_IF_COPY(ts1, 0);
	PG_FREE_IF_COPY(ts2, 1);
	PG_RETURN_POINTER(result);
}

/*****************************************************************************/

Period *
intersection_period_period_internal(Period *p1, Period *p2)
{
	TimestampTz result_lower;
	TimestampTz result_upper;
	bool result_lower_inc;
	bool result_upper_inc;

	if (!overlaps_period_period_internal(p1, p2))
		return NULL;

	if (period_cmp_bounds(p1->lower, p2->lower, true, true, 
		p1->lower_inc, p2->lower_inc) >= 0)
	{
		result_lower = p1->lower;
		result_lower_inc = p1->lower_inc;
	}
	else
	{
		result_lower = p2->lower;
		result_lower_inc = p2->lower_inc;
	}

	if (period_cmp_bounds(p1->upper, p2->upper, false, false, 
		p1->upper_inc, p2->upper_inc) <= 0)
	{
		result_upper = p1->upper;
		result_upper_inc = p1->upper_inc;
	}
	else
	{
		result_upper = p2->upper;
		result_upper_inc = p2->upper_inc;
	}

	return period_make(result_lower, result_upper, 
		result_lower_inc, result_upper_inc);
}

PG_FUNCTION_INFO_V1(intersection_period_period);

PGDLLEXPORT Datum
intersection_period_period(PG_FUNCTION_ARGS)
{
	Period *p1 = PG_GETARG_PERIOD(0);
	Period *p2 = PG_GETARG_PERIOD(1);
	Period *result = intersection_period_period_internal(p1, p2);
	if (result == NULL)
		PG_RETURN_NULL();
	PG_RETURN_PERIOD(result);
}

PeriodSet *
intersection_period_periodset_internal(Period *p, PeriodSet *ps)
{
	/* Test whether the timespan of the period set and the period overlap */
	Period *ts = periodset_bbox(ps);
	if (!overlaps_period_period_internal(p, ts))
		return NULL;

	/* Test whether period set is fully contained in the period */
	if (contained_periodset_period_internal(ps, p))
		return periodset_copy(ps);
	
	Period **periods = palloc(sizeof(Period *) * ps->count);
	int k = 0;
	for (int i = 0; i < ps->count; i++)
	{
		Period *p1 = periodset_per_n(ps, i);
		Period *p2 = intersection_period_period_internal(p1, p);
		if (p2 != NULL)
			periods[k++] = p2;			
		if (timestamp_cmp_internal(p->upper, p1->upper) < 0)
			break;
	}
	if (k == 0)
	{
		pfree(periods);
		return NULL;
	}

	PeriodSet *result = periodset_from_periodarr_internal(periods, k, false);
	
	for (int i = 0; i < k; i++)
		pfree(periods[i]);
	pfree(periods);
	
	return result;
}

PG_FUNCTION_INFO_V1(intersection_period_periodset);

PGDLLEXPORT Datum
intersection_period_periodset(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	PeriodSet *result = intersection_period_periodset_internal(p, ps);
	PG_FREE_IF_COPY(ps, 1);
	if (result == NULL)
		PG_RETURN_NULL();
	PG_RETURN_POINTER(result);
}

/*****************************************************************************/

PG_FUNCTION_INFO_V1(intersection_periodset_period);

PGDLLEXPORT Datum
intersection_periodset_period(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	PeriodSet *result = intersection_period_periodset_internal(p, ps);
	PG_FREE_IF_COPY(ps, 0);
	if (result == NULL)
		PG_RETURN_NULL();
	PG_RETURN_POINTER(result);
}

PeriodSet *
intersection_periodset_periodset_internal(PeriodSet *ps1, PeriodSet *ps2)
{
	/* Test whether the timespan of the two PeriodSet values overlap */
	Period *p1 = periodset_bbox(ps1);
	Period *p2 = periodset_bbox(ps2);
	if (!overlaps_period_period_internal(p1, p2))
		return NULL;

	Period **periods = palloc(sizeof(Period *) * (ps1->count + ps2->count));
	int i = 0, j = 0, k = 0;
	while (i < ps1->count && j < ps2->count)
	{
		Period *p1 = periodset_per_n(ps1, i);
		Period *p2 = periodset_per_n(ps2, j);
		Period *inter = intersection_period_period_internal(p1, p2);
		if (inter != NULL)
			periods[k++] = inter;
		if (timestamp_cmp_internal(p1->upper, p2->upper) == 0)
		{
			i++; j++;
		}
		else if (timestamp_cmp_internal(p1->upper, p2->upper) < 0)
			i++;
		else 
			j++;
	}
	if (k == 0)
	{
		pfree(periods);
		return NULL;
	}
	
	PeriodSet *result = periodset_from_periodarr_internal(periods, k, true);	

	for (int i = 0; i < k; i++)
		pfree(periods[i]);
	pfree(periods);

	return result;
}

PG_FUNCTION_INFO_V1(intersection_periodset_periodset);

PGDLLEXPORT Datum
intersection_periodset_periodset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps1 = PG_GETARG_PERIODSET(0);
	PeriodSet *ps2 = PG_GETARG_PERIODSET(1);
	PeriodSet *result = intersection_periodset_periodset_internal(ps1, ps2);
	PG_FREE_IF_COPY(ps1, 0);
	PG_FREE_IF_COPY(ps2, 1);
	PG_RETURN_POINTER(result);
}

/*****************************************************************************
 * Set difference
 * All internal functions produce new results that must be freed after
 *****************************************************************************/

PG_FUNCTION_INFO_V1(minus_timestamp_timestampset);

PGDLLEXPORT Datum
minus_timestamp_timestampset(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	bool contains = contains_timestampset_timestamp_internal(ts, t);
	PG_FREE_IF_COPY(ts, 1);
	if (contains)
		PG_RETURN_NULL();
	PG_RETURN_TIMESTAMPTZ(t);
}

PG_FUNCTION_INFO_V1(minus_timestamp_period);

PGDLLEXPORT Datum
minus_timestamp_period(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	Period *p = PG_GETARG_PERIOD(1);
	bool contains = contains_period_timestamp_internal(p, t);
	PG_FREE_IF_COPY(p, 1);
	if (contains)
		PG_RETURN_NULL();
	PG_RETURN_TIMESTAMPTZ(t);
}

PG_FUNCTION_INFO_V1(minus_timestamp_periodset);

PGDLLEXPORT Datum
minus_timestamp_periodset(PG_FUNCTION_ARGS)
{
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	bool contains = contains_periodset_timestamp_internal(ps, t);
	PG_FREE_IF_COPY(ps, 1);
	if (contains)
		PG_RETURN_NULL();
	PG_RETURN_TIMESTAMPTZ(t);
}

/*****************************************************************************/

TimestampSet *
minus_timestampset_timestamp_internal(TimestampSet *ts, TimestampTz t)
{
	TimestampTz *times = palloc(sizeof(Timestamp) * (ts->count));
	int k = 0;
	for (int i = 0; i < ts->count; i++)
	{
		TimestampTz t1 = timestampset_time_n(ts, i);
		if (timestamp_cmp_internal(t, t1) != 0)
			times[k++] = t1;
	}
	if (k == 0)
	{
		pfree(times);
		return NULL;
	}
	
	TimestampSet *result = timestampset_from_timestamparr_internal(times, k);

	pfree(times);

	return result;
}

PG_FUNCTION_INFO_V1(minus_timestampset_timestamp);

PGDLLEXPORT Datum
minus_timestampset_timestamp(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	TimestampSet *result = minus_timestampset_timestamp_internal(ts, t);
	PG_FREE_IF_COPY(ts, 0);
	if (result == NULL)
		PG_RETURN_NULL();
	PG_RETURN_POINTER(result);
}

TimestampSet *
minus_timestampset_timestampset_internal(TimestampSet *ts1, TimestampSet *ts2)
{
	/* Test whether the timespan of the two time values overlap */
	Period *p1 = timestampset_bbox(ts1);
	Period *p2 = timestampset_bbox(ts2);
	if (!overlaps_period_period_internal(p1, p2))
		return timestampset_copy(ts1);
	
	TimestampTz *times = palloc(sizeof(Timestamp) * ts1->count);
	int i = 0, j = 0, k = 0;
	while (i < ts1->count && j < ts2->count)
	{
		TimestampTz t1 = timestampset_time_n(ts1, i);
		TimestampTz t2 = timestampset_time_n(ts2, j);
		if (timestamp_cmp_internal(t1, t2) == 0)
		{
			i++; j++;
		}
		if (timestamp_cmp_internal(t1, t2) < 0)
		{
			times[k++] = t1;
			i++;
		}
		else
			j++;
	}
	if(k == 0)
		return NULL ;
	TimestampSet *result = timestampset_from_timestamparr_internal(times, k);
	pfree(times);
	return result;	
}

PG_FUNCTION_INFO_V1(minus_timestampset_timestampset);

PGDLLEXPORT Datum
minus_timestampset_timestampset(PG_FUNCTION_ARGS)
{
	TimestampSet *ts1 = PG_GETARG_TIMESTAMPSET(0);
	TimestampSet *ts2 = PG_GETARG_TIMESTAMPSET(1);
	TimestampSet *result = minus_timestampset_timestampset_internal(ts1, ts2);
	PG_FREE_IF_COPY(ts1, 0);
	PG_FREE_IF_COPY(ts2, 1);
	if(! result)
		PG_RETURN_NULL() ;
	PG_RETURN_POINTER(result);
}

TimestampSet *
minus_timestampset_period_internal(TimestampSet *ts, Period *p)
{
	TimestampTz *times = palloc(sizeof(Timestamp) * (ts->count + 1));
	int k = 0;
	for (int i = 0; i < ts->count; i++)
	{
		TimestampTz t = timestampset_time_n(ts, i);
		if (!contains_period_timestamp_internal(p, t))
			times[k++] = t;
	}
	if (k == 0)
	{
		pfree(times);
		return NULL;
	}
	
	TimestampSet *result = timestampset_from_timestamparr_internal(times, k);

	pfree(times);

	return result;
}

PG_FUNCTION_INFO_V1(minus_timestampset_period);

PGDLLEXPORT Datum
minus_timestampset_period(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	TimestampSet *result = minus_timestampset_period_internal(ts, p);
	PG_FREE_IF_COPY(ts, 0);
	PG_FREE_IF_COPY(p, 1);
	if (result == NULL)
		PG_RETURN_NULL();
	PG_RETURN_POINTER(result);
}

TimestampSet *
minus_timestampset_periodset_internal(TimestampSet *ts, PeriodSet *ps)
{
	/* Test whether the timespan of the two time values overlap */
	Period *p1 = timestampset_bbox(ts);
	Period *p2 = periodset_bbox(ps);
	if (!overlaps_period_period_internal(p1, p2))
		return timestampset_copy(ts);
	
	TimestampTz *times = palloc(sizeof(Timestamp) * ts->count);
	TimestampTz t = timestampset_time_n(ts, 0);
	Period *p = periodset_per_n(ps, 0);
	int i = 0, j = 0, k = 0;
	while (i < ts->count && j < ps->count)
	{
		if (timestamp_cmp_internal(t, p->lower) < 0)
		{
			times[k++] = t;
			i++;
			if (i == ts->count)
				break;
			else
				t = timestampset_time_n(ts, i);
		}
		else if (timestamp_cmp_internal(t, p->upper) > 0)
		{
			j++;
			if (j == ps->count)
				break;
			else
				p = periodset_per_n(ps, j);
		}
		else 
		{
			if (!contains_period_timestamp_internal(p, t))
				times[k++] = t;
			i++;
			if (i == ts->count)
				break;
			else
				t = timestampset_time_n(ts, i);
		}
	}
	for (int l = i; l < ts->count; l++)
		times[k++] = timestampset_time_n(ts, l);
	if (k == 0)
	{
		pfree(times);
		return NULL;
	}
	
	TimestampSet *result = timestampset_from_timestamparr_internal(times, k);

	pfree(times);

	return result;
}

PG_FUNCTION_INFO_V1(minus_timestampset_periodset);

PGDLLEXPORT Datum
minus_timestampset_periodset(PG_FUNCTION_ARGS)
{
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	TimestampSet *result = minus_timestampset_periodset_internal(ts, ps);
	PG_FREE_IF_COPY(ts, 0);
	PG_FREE_IF_COPY(ps, 1);
	if (result == NULL)
		PG_RETURN_NULL();
	PG_RETURN_POINTER(result);
}

/*****************************************************************************/

PeriodSet *
minus_period_timestamp_internal(Period *p, TimestampTz t)
{
	if (!contains_period_timestamp_internal(p, t))
		return periodset_from_periodarr_internal(&p, 1, false);

	if (timestamp_cmp_internal(p->lower, t) == 0 &&
		timestamp_cmp_internal(p->upper, t) == 0)
		return NULL;		

	if (timestamp_cmp_internal(p->lower, t) == 0)
	{
		Period *p1 = period_make(p->lower, p->upper, false, p->upper_inc);
		PeriodSet *result = periodset_from_periodarr_internal(&p1, 1, false);
		pfree(p1);
		return result;		
	}

	if (timestamp_cmp_internal(p->upper, t) == 0)
	{
		Period *p1 = period_make(p->lower, p->upper, p->lower_inc, false);
		PeriodSet *result = periodset_from_periodarr_internal(&p1, 1, false);
		pfree(p1);
		return result;		
	}

	Period **periods = palloc(sizeof(Period *) * 2);
	periods[0] = period_make(p->lower, t, p->lower_inc, false);
	periods[1] = period_make(t, p->upper, false, p->upper_inc);
	PeriodSet *result = periodset_from_periodarr_internal(periods, 2, false);
	pfree(periods[0]); pfree(periods[1]); pfree(periods);
	return result;
}

PG_FUNCTION_INFO_V1(minus_period_timestamp);

PGDLLEXPORT Datum
minus_period_timestamp(PG_FUNCTION_ARGS)
{
	Period *ps = PG_GETARG_PERIOD(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	PeriodSet *result = minus_period_timestamp_internal(ps, t);
	PG_FREE_IF_COPY(ps, 0);
	if (result == NULL)
		PG_RETURN_NULL();
	PG_RETURN_POINTER(result);
}

PeriodSet *
minus_period_timestampset_internal(Period *p, TimestampSet *ts)
{
	/* Test whether the timespan of the two time values overlap */
	Period *ts1 = timestampset_bbox(ts);
	if (!overlaps_period_period_internal(p, ts1))
		return periodset_from_periodarr_internal(&p, 1, false);
		
	Period **periods = palloc(sizeof(Period *) * (ts->count + 1));
	Period *curr = period_copy(p);
	int k = 0;
	for (int i = 0; i < ts->count; i++)
	{
		TimestampTz t = timestampset_time_n(ts, i);
		if (contains_period_timestamp_internal(curr, t))
		{
			if (timestamp_cmp_internal(curr->lower, curr->upper) == 0)
			{
				pfree(curr);
				curr = NULL;
				break;
			}
			else if (timestamp_cmp_internal(curr->lower, t) == 0)
			{
				Period *curr1 = period_make(curr->lower, curr->upper, false, curr->upper_inc);
				pfree(curr);
				curr = curr1;
			}
			else if (timestamp_cmp_internal(curr->upper, t) == 0)
			{
				Period *curr1 = period_make(curr->lower, curr->upper, curr->lower_inc, false);
				pfree(curr);
				curr = curr1;
				break;
			}
			else
			{
				periods[k++] = period_make(curr->lower, t, curr->lower_inc, false);
				Period *curr1 = period_make(t, curr->upper, false, curr->upper_inc);
				pfree(curr);
				curr = curr1;
			}
		}		
	}
	if (curr != NULL)
		periods[k++] = curr;
	if (k == 0)
	{
		pfree(periods);
		return NULL;
	}
	PeriodSet *result = periodset_from_periodarr_internal(periods, k, false);
	for (int i = 0; i < k; i++)
		pfree(periods[i]); 
	pfree(periods);
	return result;
}

PG_FUNCTION_INFO_V1(minus_period_timestampset);

PGDLLEXPORT Datum
minus_period_timestampset(PG_FUNCTION_ARGS)
{
	Period *ps = PG_GETARG_PERIOD(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	PeriodSet *result = minus_period_timestampset_internal(ps, ts);
	PG_FREE_IF_COPY(ps, 0);
	PG_FREE_IF_COPY(ts, 1);
	if (result == NULL)
		PG_RETURN_NULL();
	PG_RETURN_POINTER(result);
}

static Period **
minus_period_period_internal1(Period *p1, Period *p2, int *count)
{
	int cmp_l1l2 = period_cmp_bounds(p1->lower, p2->lower, true, true, 
		p1->lower_inc, p2->lower_inc);
	int cmp_l1u2 = period_cmp_bounds(p1->lower, p2->upper, true, false, 
		p1->lower_inc, p2->upper_inc);
	int cmp_u1l2 = period_cmp_bounds(p1->upper, p2->lower, false, true, 
		p1->upper_inc, p2->lower_inc);
	int cmp_u1u2 = period_cmp_bounds(p1->upper, p2->upper, false, false, 
		p1->upper_inc, p2->upper_inc);

	if (cmp_l1l2 >= 0 && cmp_u1u2 <= 0)
	{
		*count = 0;
		return NULL;		
	}
	
	if (cmp_l1l2 < 0 && cmp_u1u2 > 0)
	{
		Period **result = palloc(sizeof(Period *) * 2);
		result[0] = period_make(p1->lower, p2->lower, 
			p1->lower_inc, !(p2->lower_inc));
		result[1] = period_make(p2->upper, p1->upper, 
			!(p2->upper_inc), p1->upper_inc);
		*count = 2;
		return result;
	}

	Period **result = palloc(sizeof(Period *));
	if (cmp_l1u2 > 0 || cmp_u1l2 < 0)
		result[0] = period_copy(p1);
	else if (cmp_l1l2 <= 0 && cmp_u1l2 >= 0 && cmp_u1u2 <= 0)
		result[0] = period_make(p1->lower, p2->lower, p1->lower_inc, !(p2->lower_inc));
	else if (cmp_l1l2 >= 0 && cmp_u1u2 >= 0 && cmp_l1u2 <= 0)
		result[0] = period_make(p2->upper, p1->upper, !(p2->upper_inc), p1->upper_inc);
	*count = 1;
	return result;
}

PeriodSet *
minus_period_period_internal(Period *p1, Period *p2)
{
	int count;
	Period **periods = minus_period_period_internal1(p1, p2, &count);
	if (count == 0)
		return NULL;
	
	PeriodSet *result = periodset_from_periodarr_internal(periods, count, false);
	for (int i = 0; i < count; i++)
		pfree(periods[i]);
	pfree(periods);
	return result;
}

PG_FUNCTION_INFO_V1(minus_period_period);

PGDLLEXPORT Datum
minus_period_period(PG_FUNCTION_ARGS)
{
	Period *p1 = PG_GETARG_PERIOD(0);
	Period *p2 = PG_GETARG_PERIOD(1);
	PeriodSet *result = minus_period_period_internal(p1, p2);
	if (result == NULL)
		PG_RETURN_NULL();
	PG_RETURN_POINTER(result);
}

static Period **
minus_period_periodset_internal1(Period *p, Period **periods, int count, 
	int *newcount)
{
	/* The period can be split at most into (count + 1) periods
		|----------------------|
			|---| |---| |---|
	*/
	Period **newperiods = palloc(sizeof(Period *) * (count + 1));
	Period *curr = period_copy(p);
	int k = 0;
	for (int i = 0; i < count; i++)
	{
		Period *p1 = periods[i];
		/* Remaining periods are to the left of the current period */
		if (period_cmp_bounds(curr->upper, p1->lower, false, true, 
				curr->upper_inc, p1->lower_inc) < 0)
		{
			newperiods[k++] = curr; 
			break;
		}
		int countminus;
		Period **minus = minus_period_period_internal1(curr, p1, &countminus);
		pfree(curr);
		/* minus can have from 0 to 2 periods */
		if (countminus == 0)
			break;
		else if (countminus == 1) 
			curr = minus[0];
		else /* countminus == 2 */
		{
			newperiods[k++] = minus[0];
			curr = minus[1];
		}
		/* There are no more periods left */
		if (i == count - 1)
			newperiods[k++] = curr; 
	}
	*newcount = k;
	if (k == 0)
	{
		pfree(newperiods);
		return NULL;
	}
	return newperiods;
}

PeriodSet *
minus_period_periodset_internal(Period *p, PeriodSet *ps)
{
	/* Test whether the timespan of the two time values overlap */
	Period *p1 = periodset_bbox(ps);
	if (!overlaps_period_period_internal(p, p1))
		return periodset_from_periodarr_internal(&p, 1, false);

	Period **periods = periodset_periods_internal(ps);
	int count;
	Period **newperiods = minus_period_periodset_internal1(p, periods, 
		ps->count, &count);
	if (count == 0)
	{
		pfree(periods);
		return NULL;
	}
	
	PeriodSet *result = periodset_from_periodarr_internal(newperiods, count, false);

	for (int i = 0; i < count; i++)
		pfree(newperiods[i]);
	pfree(newperiods);
	pfree(periods);

	return result;
}

PG_FUNCTION_INFO_V1(minus_period_periodset);

PGDLLEXPORT Datum
minus_period_periodset(PG_FUNCTION_ARGS)
{
	Period *p = PG_GETARG_PERIOD(0);
	PeriodSet *ps = PG_GETARG_PERIODSET(1);
	PeriodSet *result = minus_period_periodset_internal(p, ps);
	PG_FREE_IF_COPY(ps, 1);
	if (result == NULL)
		PG_RETURN_NULL();
	PG_RETURN_POINTER(result);
}

/*****************************************************************************/

PeriodSet *
minus_periodset_timestamp_internal(PeriodSet *ps, TimestampTz t)
{
	/* Test whether the timespan of the two time values overlap */
	Period *p = periodset_bbox(ps);
	if (!contains_period_timestamp_internal(p, t))
		return periodset_copy(ps);

	/* At most one composing period can be split into two */
	Period **periods = palloc(sizeof(Period *) * (ps->count + 1));
	int k = 0;
	for (int i = 0; i < ps->count; i++)
	{
		Period *p = periodset_per_n(ps, i);
		PeriodSet *ps1 = minus_period_timestamp_internal(p, t);
		if (ps1 != NULL)
		{
			for (int j = 0; j < ps1->count; j++)
				periods[k++] = period_copy(periodset_per_n(ps1, j));
			pfree(ps1);
		}
	}
	if (k == 0)
	{
		pfree(periods);
		return NULL;
	}
	
	PeriodSet *result = periodset_from_periodarr_internal(periods, k, false);

	for (int i = 0; i < k; i++)
		pfree(periods[i]);
	pfree(periods);

	return result;
}

PG_FUNCTION_INFO_V1(minus_periodset_timestamp);

PGDLLEXPORT Datum
minus_periodset_timestamp(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	TimestampTz t = PG_GETARG_TIMESTAMPTZ(1);
	PeriodSet *result = minus_periodset_timestamp_internal(ps, t);
	PG_FREE_IF_COPY(ps, 0);
	if (result == NULL)
		PG_RETURN_NULL();
	PG_RETURN_POINTER(result);
}

PeriodSet *
minus_periodset_timestampset_internal(PeriodSet *ps, TimestampSet *ts)
{
	/* Test whether the timespan of the two time values overlap */
	Period *p1 = periodset_bbox(ps);
	Period *p2 = timestampset_bbox(ts);
	if (!overlaps_period_period_internal(p1, p2))
		return periodset_copy(ps);
	
	/* Each timestamp will split at most one composing period into two */
	Period **periods = palloc(sizeof(Period *) * (ps->count + ts->count + 1));
	int i = 0, j = 0, k = 0;
	Period *curr = period_copy(periodset_per_n(ps, 0));
	TimestampTz t = timestampset_time_n(ts, 0);
	while (i < ps->count && j < ts->count)
	{
		if (timestamp_cmp_internal(t, curr->upper) > 0)
		{
			periods[k++] = curr;
			i++;
			if (i == ps->count)
				break;
			else
				curr = period_copy(periodset_per_n(ps, i));
		}
		else if (timestamp_cmp_internal(t, curr->lower) < 0)
		{
			j++;
			if (j == ts->count)
				break;
			else
				t = timestampset_time_n(ts, j);
		}
		else 
		{
			if (contains_period_timestamp_internal(curr, t))
			{
				if (timestamp_cmp_internal(curr->lower, curr->upper) == 0)
				{
					pfree(curr);
					i++;
					if (i == ps->count)
						break;
					else
						curr = period_copy(periodset_per_n(ps, i));
				}
				else if (timestamp_cmp_internal(curr->lower, t) == 0)
				{
					Period *curr1 = period_make(curr->lower, curr->upper, false, curr->upper_inc);
					pfree(curr);
					curr = curr1;
				}
				else if (timestamp_cmp_internal(curr->upper, t) == 0)
				{
					periods[k++] = period_make(curr->lower, curr->upper, curr->lower_inc, false);
					pfree(curr);
					i++;
					if (i == ps->count)
						break;
					else
						curr = period_copy(periodset_per_n(ps, i));
				}
				else
				{
					periods[k++] = period_make(curr->lower, t, curr->lower_inc, false);
					Period *curr1 = period_make(t, curr->upper, false, curr->upper_inc);
					pfree(curr);
					curr = curr1;
				}
			}
			else
			{
				if (timestamp_cmp_internal(curr->upper, t) == 0)
				{
					periods[k++] = curr;
					i++;
					if (i == ps->count)
						break;
					else
						curr = period_copy(periodset_per_n(ps, i));
				}
			}
			j++;
			if (j == ts->count)
				break;
			else
				t = timestampset_time_n(ts, j);
		}	
	}
	/* if the break was because of run through all the instants */
	if (j == ts->count)
		periods[k++] = curr;
	for (int l = i+1; l < ps->count; l++)
		periods[k++] = periodset_per_n(ps, l);
		
	if (k == 0)
	{
		pfree(periods);
		return NULL;
	}
	
	PeriodSet *result = periodset_from_periodarr_internal(periods, k, false);

	for (int l = 0; l < i; l++)
		pfree(periods[l]);
	pfree(periods);

	return result;
}

PG_FUNCTION_INFO_V1(minus_periodset_timestampset);

PGDLLEXPORT Datum
minus_periodset_timestampset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	TimestampSet *ts = PG_GETARG_TIMESTAMPSET(1);
	PeriodSet *result = minus_periodset_timestampset_internal(ps, ts);
	PG_FREE_IF_COPY(ps, 0);
	PG_FREE_IF_COPY(ts, 1);
	if (result == NULL)
		PG_RETURN_NULL();
	PG_RETURN_POINTER(result);
}

PeriodSet *
minus_periodset_period_internal(PeriodSet *ps, Period *p)
{
	/* Test whether the timespan of the two time values overlap */
	Period *p1 = periodset_bbox(ps);
	if (!overlaps_period_period_internal(p1, p))
		return periodset_copy(ps);

	/* At most one composing period can be split into two */
	Period **periods = palloc(sizeof(Period *) * (ps->count + 1));
	int k = 0;
	for (int i = 0; i < ps->count; i++)
	{
		Period *p1 = periodset_per_n(ps, i);
		PeriodSet *ps1 = minus_period_period_internal(p1, p);
		if (ps1 != NULL)
		{
			for (int j = 0; j < ps1->count; j++)
				periods[k++] = period_copy(periodset_per_n(ps1, j));
			pfree(ps1);			
		}
	}
	if (k == 0)
	{
		pfree(periods);
		return NULL;
	}
	
	PeriodSet *result = periodset_from_periodarr_internal(periods, k, false);

	for (int i = 0; i < k; i++)
		pfree(periods[i]);
	pfree(periods);

	return result;
}

PG_FUNCTION_INFO_V1(minus_periodset_period);

PGDLLEXPORT Datum
minus_periodset_period(PG_FUNCTION_ARGS)
{
	PeriodSet *ps = PG_GETARG_PERIODSET(0);
	Period *p = PG_GETARG_PERIOD(1);
	PeriodSet *result = minus_periodset_period_internal(ps, p);
	PG_FREE_IF_COPY(ps, 0);
	if (result == NULL)
		PG_RETURN_NULL();
	PG_RETURN_POINTER(result);
}

PeriodSet *
minus_periodset_periodset_internal(PeriodSet *ps1, PeriodSet *ps2)
{
	/* Test whether the timespan of the two time values overlap */
	Period *p1 = periodset_bbox(ps1);
	Period *p2 = periodset_bbox(ps2);
	if (!overlaps_period_period_internal(p1, p2))
		return periodset_copy(ps1);

	Period ***periods = palloc(sizeof(Period *) * ps1->count);
	int *countpers = palloc0(sizeof(int) * ps1->count);
	int i = 0, j = 0, k = 0, totalpers = 0;
	while (i < ps1->count && j < ps2->count)
	{
		p1 = periodset_per_n(ps1, i);
		p2 = periodset_per_n(ps2, j);
		/* The periods do not overlap, copy the first period */
		if (!overlaps_period_period_internal(p1, p2))
		{
			Period *p3 = period_copy(p1);
			periods[k] = palloc(sizeof(Period *));
			*periods[k] = p3;
			countpers[k++] = 1;
			i++; totalpers++;
		}
		else
		{
			/* Find all periods in ps2 that overlap with p1 
			                  i
				|------------------------|   
				     |-----|  |-----|  
					    j        l
			*/
			Period *q2;
			int l = j;
			while (l < ps2->count)
			{
				q2 = periodset_per_n(ps2, l);
				if (overlaps_period_period_internal(p1, q2))
					l++;
				else
					break;
			}
			/* Put after the value of last increment */
			q2 = periodset_per_n(ps2, --l);
			int count = l - j + 1;
			/* Compute the difference of the overlapping periods */
			Period **newperiods = palloc(sizeof(Period *) * count);
			int countstep;
			for (int m = 0; m < count; m++)
				newperiods[m] = periodset_per_n(ps2, m + j);
			periods[k] = minus_period_periodset_internal1(p1, 
				newperiods, count, &countstep);
			countpers[k++] = countstep;
			totalpers += countstep;
			pfree(newperiods);
			i++;
			j = l+1;
		}
	}
	if (k == 0)
	{
		pfree(periods);
		return NULL;
	}
	
	if(totalpers == 0)
		return NULL ;
	k = 0;
	Period **allpers = palloc(sizeof(Period *) * totalpers);
	for (int i = 0; i < ps1->count; i++)
	{
		for (int j = 0; j < countpers[i]; j++)
			allpers[k++] = periods[i][j];
		if (countpers[i] > 0)
			pfree(periods[i]);
	}
	PeriodSet *result = periodset_from_periodarr_internal(allpers, totalpers, true);	

	for (int i = 0; i < totalpers; i++)
		pfree(allpers[i]);
	pfree(allpers);

	return result;
}

PG_FUNCTION_INFO_V1(minus_periodset_periodset);

PGDLLEXPORT Datum
minus_periodset_periodset(PG_FUNCTION_ARGS)
{
	PeriodSet *ps1 = PG_GETARG_PERIODSET(0);
	PeriodSet *ps2 = PG_GETARG_PERIODSET(1);
	PeriodSet *result = minus_periodset_periodset_internal(ps1, ps2);
	PG_FREE_IF_COPY(ps1, 0);
	PG_FREE_IF_COPY(ps2, 1);
	if(! result)
		PG_RETURN_NULL() ;
	PG_RETURN_POINTER(result);
}

/******************************************************************************/