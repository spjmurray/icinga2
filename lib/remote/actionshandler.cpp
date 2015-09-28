/******************************************************************************
 * Icinga 2                                                                   *
 * Copyright (C) 2012-2015 Icinga Development Team (http://www.icinga.org)    *
 *                                                                            *
 * This program is free software; you can redistribute it and/or              *
 * modify it under the terms of the GNU General Public License                *
 * as published by the Free Software Foundation; either version 2             *
 * of the License, or (at your option) any later version.                     *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software Foundation     *
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ******************************************************************************/

#include "remote/actionshandler.hpp"
#include "remote/httputility.hpp"
#include "remote/filterutility.hpp"
#include "remote/apiaction.hpp"
#include "base/exception.hpp"
#include "base/serializer.hpp"
#include "base/logger.hpp"
#include <boost/algorithm/string.hpp>
#include <set>

using namespace icinga;

REGISTER_URLHANDLER("/v1/actions", ActionsHandler);

bool ActionsHandler::HandleRequest(const ApiUser::Ptr& user, HttpRequest& request, HttpResponse& response)
{
	if (request.RequestMethod != "POST") {
		HttpUtility::SendJsonError(response, 400, "Invalid request type. Must be POST.");
		return true;
	}

	if (request.RequestUrl->GetPath().size() < 3) {
		HttpUtility::SendJsonError(response, 400, "Action is missing.");
		return true;
	}

	String actionName = request.RequestUrl->GetPath()[2];

	ApiAction::Ptr action = ApiAction::GetByName(actionName);

	if (!action) {
		HttpUtility::SendJsonError(response, 404, "Action '" + actionName + "' could not be found.");
		return true;
	}

	QueryDescription qd;

	Dictionary::Ptr params = HttpUtility::FetchRequestParameters(request);

	const std::vector<String>& types = action->GetTypes();
	std::vector<Value> objs;

	if (!types.empty()) {
		qd.Types = std::set<String>(types.begin(), types.end());

		try {
			objs = FilterUtility::GetFilterTargets(qd, params);
		} catch (const std::exception& ex) {
			HttpUtility::SendJsonError(response, 400,
			    "Type/Filter was required but not provided or was invalid.",
			    request.GetVerboseErrors() ? DiagnosticInformation(ex) : "");
			return true;
		}
	} else
		objs.push_back(ConfigObject::Ptr());

	Array::Ptr results = new Array();

	Log(LogNotice, "ApiActionHandler")
	    << "Running action " << actionName;

	BOOST_FOREACH(const ConfigObject::Ptr& obj, objs) {
		try {
			results->Add(action->Invoke(obj, params));
		} catch (const std::exception& ex) {
			Dictionary::Ptr fail = new Dictionary();
			fail->Set("code", 500);
			fail->Set("status", "Action execution failed.");
			if (request.GetVerboseErrors())
				fail->Set("diagnostic information", DiagnosticInformation(ex));
			results->Add(fail);
		}
	}

	Dictionary::Ptr result = new Dictionary();
	result->Set("results", results);

	response.SetStatus(200, "OK");
	HttpUtility::SendJsonBody(response, result);

	return true;
}
