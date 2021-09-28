# Copyright 2018 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import datetime
import inspect
import logging
import json

import endpoints
from protorpc import messages
from protorpc import remote
from google.appengine.ext import ndb

from webapp.src import vtslab_status as Status
from webapp.src.proto import model

MAX_QUERY_SIZE = 1000

COUNT_REQUEST_RESOURCE = endpoints.ResourceContainer(model.CountRequestMessage)
GET_REQUEST_RESOURCE = endpoints.ResourceContainer(model.GetRequestMessage)


class EndpointBase(remote.Service):
    """A base class for endpoint implementation."""

    def GetCommonAttributes(self, resource, reference):
        """Gets a list of common attribute names.

        This method finds the attributes assigned in 'resource' instance, and
        filters out if the attributes are not a member of 'reference' class.

        Args:
            resource: either a protorpc.messages.Message instance,
                      or a ndb.Model instance.
            reference: either a protorpc.messages.Message class,
                       or a ndb.Model class.

        Returns:
            a list of string, attribute names exist on resource and reference.

        Raises:
            ValueError if resource or reference is not supported class.
        """
        # check resource type and absorb list of assigned attributes.
        resource_attrs = self.GetAttributes(resource, assigned_only=True)
        reference_attrs = self.GetAttributes(reference)
        return [x for x in resource_attrs if x in reference_attrs]

    def GetAttributes(self, value, assigned_only=False):
        """Gets a list of attributes.

        Args:
            value: a class instance or a class itself.
            assigned_only: True to get only assigned attributes when value is
                           an instance, False to get all attributes.

        Raises:
            ValueError if value is not supported class.
        """
        attrs = []
        if inspect.isclass(value):
            if assigned_only:
                logging.warning(
                    "Please use a class instance for 'resource' argument.")

            if (issubclass(value, messages.Message)
                    or issubclass(value, ndb.Model)):
                attrs = [
                    x[0] for x in value.__dict__.items()
                    if not x[0].startswith("_")
                ]
            else:
                raise ValueError("Only protorpc.messages.Message or ndb.Model "
                                 "class are supported.")
        else:
            if isinstance(value, messages.Message):
                attrs = [
                    x.name for x in value.all_fields()
                    if not assigned_only or (
                        value.get_assigned_value(x.name) not in [None, []])
                ]
            elif isinstance(value, ndb.Model):
                attrs = [
                    x for x in list(value.to_dict())
                    if not assigned_only or (
                        getattr(value, x, None) not in [None, []])
                ]
            else:
                raise ValueError("Only protorpc.messages.Message or ndb.Model "
                                 "class are supported.")

        return attrs

    def Count(self, metaclass, filters=None):
        """Counts entities from datastore with options.

        Args:
            metaclass: a metaclass for ndb model.
            filters: a list of tuples. Each tuple consists of three values:
                     key, method, and value.

        Returns:
            a number of entities.
        """
        query, _ = self.CreateQueryFilter(metaclass=metaclass, filters=filters)
        return query.count()

    def Fetch(self,
              metaclass,
              size,
              offset=0,
              filters=None,
              sort_key="",
              direction="asc"):
        """Fetches entities from datastore with options.

        Args:
            metaclass: a metaclass for ndb model.
            size: an integer, max number of entities to fetch at once.
            offset: an integer, number of query results to skip.
            filters: a list of filter tuple, a form of (key: string,
                     method: integer, value: string).
            sort_key: a string, key name to sort by.
            direction: a string, "asc" for ascending order and "desc" for
                       descending order.

        Returns:
            a list of fetched entities.
            a boolean, True if there is next page or False if not.
        """
        query, empty_repeated_field = self.CreateQueryFilter(
            metaclass=metaclass, filters=filters)
        sorted_query = self.SortQuery(
            query=query,
            metaclass=metaclass,
            sort_key=sort_key,
            direction=direction)

        if size:
            entities, _, more = sorted_query.fetch_page(
                page_size=size, offset=offset)
        else:
            entities = sorted_query.fetch()
            more = False

        if empty_repeated_field:
            entities = [
                x for x in entities
                if all([not getattr(x, attr) for attr in empty_repeated_field])
            ]

        return entities, more

    def CreateQueryFilter(self, metaclass, filters):
        """Creates a query with the given filters.

        Args:
            metaclass: a metaclass for ndb model.
            filters: a list of tuples. Each tuple consists of three values:
                     key, method, and value.

        Returns:
            a filtered query for the given metaclass.
            a list of strings that failed to create the query due to its empty
            value for the repeated property.
        """
        empty_repeated_field = []
        query = metaclass.query()
        if not filters:
            return query, empty_repeated_field

        for _filter in filters:
            property_key = _filter["key"]
            method = _filter["method"]
            value = _filter["value"]
            if type(value) is str or type(value) is unicode:
                if isinstance(metaclass._properties[property_key],
                              ndb.BooleanProperty):
                    value = value.lower() in ("yes", "true", "1")
                elif isinstance(metaclass._properties[property_key],
                                ndb.IntegerProperty):
                    value = int(value)
            if metaclass._properties[property_key]._repeated:
                if value:
                    value = [value]
                    if method == Status.FILTER_METHOD[Status.FILTER_Has]:
                        query = query.filter(
                            getattr(metaclass, property_key).IN(value))
                    else:
                        logging.warning(
                            "You cannot compare repeated "
                            "properties except 'IN(has)' operation.")
                else:
                    logging.debug("Empty repeated list cannot be queried.")
                    empty_repeated_field.append(value)
            elif isinstance(metaclass._properties[property_key],
                            ndb.DateTimeProperty):
                if method == Status.FILTER_METHOD[Status.FILTER_LessThan]:
                    query = query.filter(
                        getattr(metaclass, property_key) < datetime.datetime.
                        now() - datetime.timedelta(hours=int(value)))
                elif method == Status.FILTER_METHOD[Status.FILTER_GreaterThan]:
                    query = query.filter(
                        getattr(metaclass, property_key) > datetime.datetime.
                        now() - datetime.timedelta(hours=int(value)))
                else:
                    logging.debug("DateTimeProperty only allows <=(less than) "
                                  "and >=(greater than) operation.")
            else:
                if method == Status.FILTER_METHOD[Status.FILTER_EqualTo]:
                    query = query.filter(
                        getattr(metaclass, property_key) == value)
                elif method == Status.FILTER_METHOD[Status.FILTER_LessThan]:
                    query = query.filter(
                        getattr(metaclass, property_key) < value)
                elif method == Status.FILTER_METHOD[Status.FILTER_GreaterThan]:
                    query = query.filter(
                        getattr(metaclass, property_key) > value)
                elif method == Status.FILTER_METHOD[
                        Status.FILTER_LessThanOrEqualTo]:
                    query = query.filter(
                        getattr(metaclass, property_key) <= value)
                elif method == Status.FILTER_METHOD[
                        Status.FILTER_GreaterThanOrEqualTo]:
                    query = query.filter(
                        getattr(metaclass, property_key) >= value)
                elif method == Status.FILTER_METHOD[Status.FILTER_NotEqualTo]:
                    query = query.filter(
                        getattr(metaclass, property_key) != value).order(
                            getattr(metaclass, property_key), metaclass.key)
                elif method == Status.FILTER_METHOD[Status.FILTER_Has]:
                    query = query.filter(
                        getattr(metaclass, property_key).IN(value)).order(
                            getattr(metaclass, property_key), metaclass.key)
                else:
                    logging.warning(
                        "{} is not supported filter method.".format(method))
        return query, empty_repeated_field

    def SortQuery(self, query, metaclass, sort_key, direction):
        """Sorts the given query with sort_key and direction.

        Args:
            query: a ndb query to sort.
            metaclass: a metaclass for ndb model.
            sort_key: a string, key name to sort by.
            direction: a string, "asc" for ascending order and "desc" for
                       descending order.
        """
        if sort_key:
            if direction == "desc":
                query = query.order(-getattr(metaclass, sort_key))
            else:
                query = query.order(getattr(metaclass, sort_key))

        return query

    def CreateFilterList(self, filter_string, metaclass):
        """Creates a list of filters.

        Args:
            filter_string: a string, stringified JSON which contains 'key',
                           'method', 'value' to build filter information.
            metaclass: a metaclass for ndb model.

        Returns:
            a list of tuples where each tuple consists of three values:
            key, method, and value.
        """
        model_properties = self.GetAttributes(metaclass)
        filters = []
        if filter_string:
            filters = json.loads(filter_string)
            for _filter in filters:
                if _filter["key"] not in model_properties:
                    filters.remove(_filter)
        return filters

    def Get(self, request, metaclass, message):
        """Handles a request through /get endpoints API to retrieves entities.

        Args:
            request: a request body message received through /get API.
            metaclass: a metaclass for ndb model. This method will fetch the
                       'metaclass' type of model from datastore.
            message: a Protocol RPC message class. Fetched entities will be
                     converted to this message class instances.

        Returns:
            a list of fetched entities.
            a boolean, True if there is next page or False if not.
        """
        size = request.size if request.size else MAX_QUERY_SIZE
        offset = request.offset if request.offset else 0

        filters = self.CreateFilterList(
            filter_string=request.filter, metaclass=metaclass)

        entities, more = self.Fetch(
            metaclass=metaclass,
            size=size,
            filters=filters,
            offset=offset,
            sort_key=request.sort,
            direction=request.direction,
        )

        return_list = []
        for entity in entities:
            entity_dict = {}
            assigned_attributes = self.GetCommonAttributes(
                resource=entity, reference=message)
            for attr in assigned_attributes:
                entity_dict[attr] = getattr(entity, attr, None)
            if hasattr(message, "urlsafe_key"):
                entity_dict["urlsafe_key"] = entity.key.urlsafe()
            return_list.append(entity_dict)

        return return_list, more
