/*
 * Licensed to Green Energy Corp (www.greenenergycorp.com) under one or
 * more contributor license agreements. See the NOTICE file distributed
 * with this work for additional information regarding copyright ownership.
 * Green Energy Corp licenses this file to you under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This project was forked on 01/01/2013 by Automatak, LLC and modifications
 * may have been made to this file. Automatak, LLC licenses these modifications
 * to you under the terms of the License.
 */

#include "opendnp3/master/RestartOperationTask.h"

#include "opendnp3/master/TaskPriority.h"
#include "opendnp3/app/parsing/APDUParser.h"
#include "opendnp3/app/APDUBuilders.h"

using namespace openpal;

namespace opendnp3
{

RestartOperationTask::RestartOperationTask(const std::shared_ptr<TaskContext>& context, IMasterApplication& app, const openpal::MonotonicTimestamp& startTimeout, RestartType operationType, const RestartOperationCallbackT& callback, openpal::Logger logger, const TaskConfig& config) :
	IMasterTask(context, app, TaskBehavior::SingleExecutionNoRetry(startTimeout), logger, config),
	function((operationType == RestartType::COLD) ? FunctionCode::COLD_RESTART : FunctionCode::WARM_RESTART),
	callback(callback)
{

}

bool RestartOperationTask::BuildRequest(APDURequest& request, uint8_t seq)
{
	request.SetControl(AppControlField(true, true, false, false, seq));
	request.SetFunction(this->function);
	return true;
}

bool RestartOperationTask::IsAllowed(uint32_t headerCount, GroupVariation gv, QualifierCode qc)
{
	if (headerCount != 0)
	{
		return false;
	}

	switch (gv)
	{
	case(GroupVariation::Group52Var1) :
	case(GroupVariation::Group52Var2) :
		return true;
	default:
		return false;
	}
}

MasterTaskType RestartOperationTask::GetTaskType() const
{
	return MasterTaskType::USER_TASK;
}

char const* RestartOperationTask::Name() const
{
	return FunctionCodeToString(this->function);
}

IMasterTask::ResponseResult RestartOperationTask::ProcessResponse(const opendnp3::APDUResponseHeader& header, const openpal::RSlice& objects)
{
	if (!(ValidateSingleResponse(header) && ValidateInternalIndications(header)))
	{
		return ResponseResult::ERROR_BAD_RESPONSE;
	}

	if (objects.IsEmpty())
	{
		return ResponseResult::ERROR_BAD_RESPONSE;
	}

	auto result = APDUParser::Parse(objects, *this, &logger);

	return (result == ParseResult::OK) ? ResponseResult::OK_FINAL : ResponseResult::ERROR_BAD_RESPONSE;
}

IINField RestartOperationTask::ProcessHeader(const CountHeader& header, const ICollection<Group52Var1>& values)
{
	Group52Var1 value;
	if (values.ReadOnlyValue(value))
	{
		this->duration = TimeDuration::Seconds(value.time);
		return IINField::Empty();
	}
	else
	{
		return IINBit::PARAM_ERROR;
	}
}

IINField RestartOperationTask::ProcessHeader(const CountHeader& header, const ICollection<Group52Var2>& values)
{
	Group52Var2 value;
	if (values.ReadOnlyValue(value))
	{
		this->duration = TimeDuration::Milliseconds(value.time);
		return IINField::Empty();
	}
	else
	{
		return IINBit::PARAM_ERROR;
	}
}

FunctionCode RestartOperationTask::ToFunctionCode(RestartType op)
{
	return (op == RestartType::COLD) ? FunctionCode::COLD_RESTART : FunctionCode::WARM_RESTART;
}

void RestartOperationTask::OnTaskComplete(TaskCompletion result, openpal::MonotonicTimestamp now)
{
	if (this->Errors().Any())
	{
		this->callback(RestartOperationResult(TaskCompletion::FAILURE_BAD_RESPONSE, this->duration));
	}
	else
	{
		this->callback(RestartOperationResult(result, this->duration));
	}

}

} //end ns



