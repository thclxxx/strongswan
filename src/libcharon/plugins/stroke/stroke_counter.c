/*
 * Copyright (C) 2012 Martin Willi
 * Copyright (C) 2012 revosec AG
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include "stroke_counter.h"

#include <threading/spinlock.h>

ENUM(stroke_counter_type_names,
	COUNTER_INIT_IKE_SA_REKEY, COUNTER_OUT_INFORMATIONAL_RSP,
	"ikeInitRekey",
	"ikeRspRekey",
	"ikeChildSaRekey",
	"ikeInInvalid",
	"ikeInInvalidSpi",
	"ikeInInitReq",
	"ikeInInitRsp",
	"ikeOutInitReq",
	"ikeOutInitRsp",
	"ikeInAuthReq",
	"ikeInAuthRsp",
	"ikeOutAuthReq",
	"ikeOutAuthRsp",
	"ikeInCrChildReq",
	"ikeInCrChildRsp",
	"ikeOutCrChildReq",
	"ikeOutCrChildRsp",
	"ikeInInfoReq",
	"ikeInInfoRsp",
	"ikeOutInfoReq",
	"ikeOutInfoRsp",
);

typedef struct private_stroke_counter_t private_stroke_counter_t;

/**
 * Private data of an stroke_counter_t object.
 */
struct private_stroke_counter_t {

	/**
	 * Public stroke_counter_t interface.
	 */
	stroke_counter_t public;

	/**
	 * Counter values
	 */
	u_int64_t counter[COUNTER_MAX];

	/**
	 * Lock for counter values
	 */
	spinlock_t *lock;
};

METHOD(listener_t, alert, bool,
	private_stroke_counter_t *this, ike_sa_t *ike_sa,
	alert_t alert, va_list args)
{
	stroke_counter_type_t type;

	switch (alert)
	{
		case ALERT_INVALID_IKE_SPI:
			type = COUNTER_IN_INVALID_IKE_SPI;
			break;
		case ALERT_PARSE_ERROR_HEADER:
		case ALERT_PARSE_ERROR_BODY:
			type = COUNTER_IN_INVALID;
			break;
		default:
			return TRUE;
	}

	this->lock->lock(this->lock);
	this->counter[type]++;
	this->lock->unlock(this->lock);

	return TRUE;
}

METHOD(listener_t, ike_rekey, bool,
	private_stroke_counter_t *this, ike_sa_t *old, ike_sa_t *new)
{
	stroke_counter_type_t type;
	ike_sa_id_t *id;

	id = new->get_id(new);
	if (id->is_initiator(id))
	{
		type = COUNTER_INIT_IKE_SA_REKEY;
	}
	else
	{
		type = COUNTER_RESP_IKE_SA_REKEY;
	}

	this->lock->lock(this->lock);
	this->counter[type]++;
	this->lock->unlock(this->lock);

	return TRUE;
}

METHOD(listener_t, child_rekey, bool,
	private_stroke_counter_t *this, ike_sa_t *ike_sa,
	child_sa_t *old, child_sa_t *new)
{
	this->lock->lock(this->lock);
	this->counter[COUNTER_CHILD_SA_REKEY]++;
	this->lock->unlock(this->lock);

	return TRUE;
}

METHOD(stroke_counter_t, destroy, void,
	private_stroke_counter_t *this)
{
	this->lock->destroy(this->lock);
	free(this);
}

/**
 * See header
 */
stroke_counter_t *stroke_counter_create()
{
	private_stroke_counter_t *this;

	INIT(this,
		.public = {
			.listener = {
				.alert = _alert,
				.ike_rekey = _ike_rekey,
				.child_rekey = _child_rekey,
			},
			.destroy = _destroy,
		},
		.lock = spinlock_create(),
	);

	return &this->public;
}
