/* GVM
 * $Id$
 * Description: GVM GMP layer: Tickets.
 *
 * Authors:
 * Matthew Mundell <matthew.mundell@greenbone.net>
 *
 * Copyright:
 * Copyright (C) 2018 Greenbone Networks GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * @file gmp_tickets.c
 * @brief GVM GMP layer: Tickets
 *
 * GMP tickets.
 */

#include "gmp_tickets.h"
#include "gmp_base.h"
#include "gmp_get.h"
#include "manage_tickets.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>

#include <gvm/util/xmlutils.h>

#undef G_LOG_DOMAIN
/**
 * @brief GLib log domain.
 */
#define G_LOG_DOMAIN "md    gmp"


/* GET_TICKETS. */

/**
 * @brief The get_tickets command.
 */
typedef struct
{
  get_data_t get;    ///< Get args.
} get_tickets_t;

/**
 * @brief Parser callback data.
 *
 * This is initially 0 because it's a global variable.
 */
static get_tickets_t get_tickets_data;

/**
 * @brief Reset command data.
 */
static void
get_tickets_reset ()
{
  get_data_reset (&get_tickets_data.get);
  memset (&get_tickets_data, 0, sizeof (get_tickets_t));
}

/**
 * @brief Handle command start element.
 *
 * @param[in]  attribute_names   All attribute names.
 * @param[in]  attribute_values  All attribute values.
 */
void
get_tickets_start (const gchar **attribute_names,
                   const gchar **attribute_values)
{
  get_data_parse_attributes (&get_tickets_data.get, "ticket",
                             attribute_names,
                             attribute_values);
}

/**
 * @brief Handle end element.
 *
 * @param[in]  gmp_parser   GMP parser.
 * @param[in]  error        Error parameter.
 */
void
get_tickets_run (gmp_parser_t *gmp_parser, GError **error)
{
  iterator_t tickets;
  int count, filtered, ret, first;

  count = 0;

  ret = init_get ("get_tickets",
                  &get_tickets_data.get,
                  "Tickets",
                  &first);
  if (ret)
    {
      switch (ret)
        {
          case 99:
            SEND_TO_CLIENT_OR_FAIL
             (XML_ERROR_SYNTAX ("get_tickets",
                                "Permission denied"));
            break;
          default:
            internal_error_send_to_client (error);
            get_tickets_reset ();
            return;
        }
      get_tickets_reset ();
      return;
    }

  ret = init_ticket_iterator (&tickets, &get_tickets_data.get);
  if (ret)
    {
      switch (ret)
        {
          case 1:
            if (send_find_error_to_client ("get_tickets",
                                           "ticket",
                                           get_tickets_data.get.id,
                                           gmp_parser))
              {
                error_send_to_client (error);
                get_tickets_reset ();
                return;
              }
            break;
          case 2:
            if (send_find_error_to_client
                  ("get_tickets", "filter",
                   get_tickets_data.get.filt_id, gmp_parser))
              {
                error_send_to_client (error);
                get_tickets_reset ();
                return;
              }
            break;
          case -1:
            SEND_TO_CLIENT_OR_FAIL
              (XML_INTERNAL_ERROR ("get_tickets"));
            break;
        }
      get_tickets_reset ();
      return;
    }

  SEND_GET_START ("ticket");
  while (1)
    {
      iterator_t results;

      ret = get_next (&tickets, &get_tickets_data.get, &first,
                      &count, init_ticket_iterator);
      if (ret == 1)
        break;
      if (ret == -1)
        {
          internal_error_send_to_client (error);
          get_tickets_reset ();
          return;
        }

      SEND_GET_COMMON (ticket, &get_tickets_data.get, &tickets);

      SENDF_TO_CLIENT_OR_FAIL ("<assigned_to>"
                               "<user id=\"%s\"/>"
                               "</assigned_to>"
                               "<task id=\"%s\"/>"
                               "<report id=\"%s\"/>"
                               "<severity>%1.1f</severity>"
                               "<host>%s</host>"
                               "<location>%s</location>"
                               "<solution_type>%s</solution_type>"
                               "<status>%s</status>"
                               "<open_time>%s</open_time>",
                               ticket_iterator_user_id (&tickets),
                               ticket_iterator_task_id (&tickets),
                               ticket_iterator_report_id (&tickets),
                               ticket_iterator_severity (&tickets),
                               ticket_iterator_host (&tickets),
                               ticket_iterator_location (&tickets),
                               ticket_iterator_solution_type (&tickets),
                               ticket_iterator_status (&tickets),
                               ticket_iterator_open_time (&tickets));

      if (ticket_iterator_solved_time (&tickets))
        {
          SENDF_TO_CLIENT_OR_FAIL ("<solved_time>%s</solved_time>",
                                   ticket_iterator_solved_time (&tickets));
          SENDF_TO_CLIENT_OR_FAIL ("<solved_comment>%s</solved_comment>",
                                   ticket_iterator_solved_comment (&tickets));
        }

      if (ticket_iterator_closed_time (&tickets))
        {
          SENDF_TO_CLIENT_OR_FAIL ("<closed_time>%s</closed_time>",
                                   ticket_iterator_closed_time (&tickets));
          SENDF_TO_CLIENT_OR_FAIL ("<closed_comment>%s</closed_comment>",
                                   ticket_iterator_closed_comment (&tickets));
        }

      if (ticket_iterator_confirmed_time (&tickets))
        {
          SENDF_TO_CLIENT_OR_FAIL ("<confirmed_time>%s</confirmed_time>",
                                   ticket_iterator_confirmed_time (&tickets));
          if (ticket_iterator_confirmed_report_id (&tickets))
            SENDF_TO_CLIENT_OR_FAIL ("<confirmed_report>"
                                     "<report id=\"%s\"/>"
                                     "</confirmed_report>",
                                     ticket_iterator_confirmed_report_id
                                      (&tickets));
        }

      if (init_ticket_result_iterator (&results,
                                       get_iterator_uuid (&tickets),
                                       get_tickets_data.get.trash))
        {
          internal_error_send_to_client (error);
          get_tickets_reset ();
          return;
        }
      while (next (&results))
        SENDF_TO_CLIENT_OR_FAIL ("<result id=\"%s\"/>",
                                 ticket_result_iterator_result_id (&results));
      cleanup_iterator (&results);

      SEND_TO_CLIENT_OR_FAIL ("</ticket>");
      count++;
    }
  cleanup_iterator (&tickets);
  filtered = get_tickets_data.get.id
              ? 1
              : ticket_count (&get_tickets_data.get);
  SEND_GET_END ("ticket", &get_tickets_data.get, count, filtered);

  get_tickets_reset ();
}


/* CREATE_TICKET. */

#if 0
/**
 * @brief Command layout.
 */
typedef struct
{
  gchar *name,
  spec_t elements[]
} spec_t;

spec_t spec = {
                "create_ticket",
                [
                  { "name", [] },
                  { "comment", [] },
                  { "copy", [] },
                  { "result", [] },
                  { NULL, [] }
                ]
              };
#endif

/**
 * @brief The create_ticket command.
 */
typedef struct
{
  context_data_t *context;     ///< XML parser context.
} create_ticket_t;

/**
 * @brief Parser callback data.
 *
 * This is initially 0 because it's a global variable.
 */
static create_ticket_t create_ticket_data;

/**
 * @brief Reset command data.
 */
static void
create_ticket_reset ()
{
  if (create_ticket_data.context->first)
    {
      free_entity (create_ticket_data.context->first->data);
      g_slist_free_1 (create_ticket_data.context->first);
    }
  g_free (create_ticket_data.context);
  memset (&create_ticket_data, 0, sizeof (get_tickets_t));
}

/**
 * @brief Start a command.
 *
 * @param[in]  gmp_parser        GMP parser.
 * @param[in]  attribute_names   All attribute names.
 * @param[in]  attribute_values  All attribute values.
 */
void
create_ticket_start (gmp_parser_t *gmp_parser,
                     const gchar **attribute_names,
                     const gchar **attribute_values)
{
  memset (&create_ticket_data, 0, sizeof (get_tickets_t));
  create_ticket_data.context = g_malloc0 (sizeof (context_data_t));
  create_ticket_element_start (gmp_parser, "create_ticket", attribute_names,
                               attribute_values);
}

/**
 * @brief Start element.
 *
 * @param[in]  gmp_parser        GMP parser.
 * @param[in]  name              Element name.
 * @param[in]  attribute_names   All attribute names.
 * @param[in]  attribute_values  All attribute values.
 */
void
create_ticket_element_start (gmp_parser_t *gmp_parser, const gchar *name,
                             const gchar **attribute_names,
                             const gchar **attribute_values)
{
  //element_start (&spec, create_ticket_data.context...);
  xml_handle_start_element (create_ticket_data.context, name, attribute_names,
                            attribute_values);
}

/**
 * @brief Execute command.
 *
 * @param[in]  gmp_parser   GMP parser.
 * @param[in]  error        Error parameter.
 */
void
create_ticket_run (gmp_parser_t *gmp_parser, GError **error)
{
  entity_t entity, copy, comment, result, assigned_to, user;
  ticket_t new_ticket;
  const char *result_id, *user_id;

  entity = (entity_t) create_ticket_data.context->first->data;

  copy = entity_child (entity, "copy");

  if (copy)
    {
      comment = entity_child (entity, "comment");
      switch (copy_ticket (comment ? entity_text (comment) : "",
                           entity_text (copy),
                           &new_ticket))
        {
          case 0:
            {
              char *uuid;
              uuid = ticket_uuid (new_ticket);
              SENDF_TO_CLIENT_OR_FAIL (XML_OK_CREATED_ID ("create_ticket"),
                                       uuid);
              log_event ("ticket", "Ticket", uuid, "created");
              free (uuid);
              break;
            }
          case 1:
            SEND_TO_CLIENT_OR_FAIL
             (XML_ERROR_SYNTAX ("create_ticket",
                                "Ticket exists already"));
            log_event_fail ("ticket", "Ticket", NULL, "created");
            break;
          case 2:
            if (send_find_error_to_client ("create_ticket", "ticket",
                                           entity_text (copy),
                                           gmp_parser))
              {
                error_send_to_client (error);
                return;
              }
            log_event_fail ("ticket", "Ticket", NULL, "created");
            break;
          case 99:
            SEND_TO_CLIENT_OR_FAIL
             (XML_ERROR_SYNTAX ("create_ticket",
                                "Permission denied"));
            log_event_fail ("ticket", "Ticket", NULL, "created");
            break;
          case -1:
          default:
            SEND_TO_CLIENT_OR_FAIL
             (XML_INTERNAL_ERROR ("create_ticket"));
            log_event_fail ("ticket", "Ticket", NULL, "created");
            break;
        }
      create_ticket_reset ();
      return;
    }

  comment = entity_child (entity, "comment");

  result = entity_child (entity, "result");
  if (result == NULL)
    {
      SEND_TO_CLIENT_OR_FAIL
       (XML_ERROR_SYNTAX ("create_ticket",
                          "CREATE_TICKET requires a RESULT"));
      create_ticket_reset ();
      return;
    }

  assigned_to = entity_child (entity, "assigned_to");
  if (assigned_to == NULL)
    {
      SEND_TO_CLIENT_OR_FAIL
       (XML_ERROR_SYNTAX ("create_ticket",
                          "CREATE_TICKET requires an ASSIGNED_TO element"));
      create_ticket_reset ();
      return;
    }

  user = entity_child (assigned_to, "user");
  if (user == NULL)
    {
      SEND_TO_CLIENT_OR_FAIL
       (XML_ERROR_SYNTAX ("create_ticket",
                          "CREATE_TICKET requires USER in ASSIGNED_TO"));
      create_ticket_reset ();
      return;
    }

  result_id = entity_attribute (result, "id");
  user_id = entity_attribute (user, "id");

  if ((result_id == NULL) || (strlen (result_id) == 0))
    SEND_TO_CLIENT_OR_FAIL
     (XML_ERROR_SYNTAX ("create_ticket",
                        "CREATE_TICKET RESULT must have an id"
                        " attribute"));
  else if ((user_id == NULL) || (strlen (user_id) == 0))
    SEND_TO_CLIENT_OR_FAIL
     (XML_ERROR_SYNTAX ("create_ticket",
                        "CREATE_TICKET USER must have an id"
                        " attribute"));
  else switch (create_ticket
                (comment ? entity_text (comment) : "",
                 result_id,
                 user_id,
                 &new_ticket))
    {
      case 0:
        {
          char *uuid = ticket_uuid (new_ticket);
          SENDF_TO_CLIENT_OR_FAIL (XML_OK_CREATED_ID ("create_ticket"),
                                   uuid);
          log_event ("ticket", "Ticket", uuid, "created");
          free (uuid);
          break;
        }
      case 1:
        log_event_fail ("ticket", "Ticket", NULL, "created");
        if (send_find_error_to_client ("create_ticket", "user", user_id,
                                       gmp_parser))
          {
            error_send_to_client (error);
            return;
          }
        break;
      case 2:
        log_event_fail ("ticket", "Ticket", NULL, "created");
        if (send_find_error_to_client ("create_ticket", "result", result_id,
                                       gmp_parser))
          {
            error_send_to_client (error);
            return;
          }
        break;
      case 99:
        SEND_TO_CLIENT_OR_FAIL
         (XML_ERROR_SYNTAX ("create_ticket",
                            "Permission denied"));
        log_event_fail ("ticket", "Ticket", NULL, "created");
        break;
      case -1:
      default:
        SEND_TO_CLIENT_OR_FAIL (XML_INTERNAL_ERROR ("create_ticket"));
        log_event_fail ("ticket", "Ticket", NULL, "created");
        break;
    }

  create_ticket_reset ();
}

/**
 * @brief End element.
 *
 * @param[in]  gmp_parser   GMP parser.
 * @param[in]  error        Error parameter.
 * @param[in]  name         Element name.
 *
 * @return 0 success, 1 command finished.
 */
int
create_ticket_element_end (gmp_parser_t *gmp_parser, GError **error,
                           const gchar *name)
{
  //element_end (&spec, create_ticket_data.context...);
  xml_handle_end_element (create_ticket_data.context, name);
  if (create_ticket_data.context->done)
    {
      create_ticket_run (gmp_parser, error);
      return 1;
    }
  return 0;
}

/**
 * @brief Add text to element.
 *
 * @param[in]  text         Text.
 * @param[in]  text_len     Text length.
 */
void
create_ticket_element_text (const gchar *text, gsize text_len)
{
  //element_text (&spec, create_ticket_data.context...);
  xml_handle_text (create_ticket_data.context, text, text_len);
}


/* MODIFY_TICKET. */

/**
 * @brief The modify_ticket command.
 */
typedef struct
{
  context_data_t *context;     ///< XML parser context.
} modify_ticket_t;

/**
 * @brief Parser callback data.
 *
 * This is initially 0 because it's a global variable.
 */
static modify_ticket_t modify_ticket_data;

/**
 * @brief Reset command data.
 */
static void
modify_ticket_reset ()
{
  if (modify_ticket_data.context->first)
    {
      free_entity (modify_ticket_data.context->first->data);
      g_slist_free_1 (modify_ticket_data.context->first);
    }
  g_free (modify_ticket_data.context);
  memset (&modify_ticket_data, 0, sizeof (get_tickets_t));
}

/**
 * @brief Start a command.
 *
 * @param[in]  gmp_parser        GMP parser.
 * @param[in]  attribute_names   All attribute names.
 * @param[in]  attribute_values  All attribute values.
 */
void
modify_ticket_start (gmp_parser_t *gmp_parser,
                     const gchar **attribute_names,
                     const gchar **attribute_values)
{
  memset (&modify_ticket_data, 0, sizeof (get_tickets_t));
  modify_ticket_data.context = g_malloc0 (sizeof (context_data_t));
  modify_ticket_element_start (gmp_parser, "modify_ticket", attribute_names,
                               attribute_values);
}

/**
 * @brief Start element.
 *
 * @param[in]  gmp_parser        GMP parser.
 * @param[in]  name              Element name.
 * @param[in]  attribute_names   All attribute names.
 * @param[in]  attribute_values  All attribute values.
 */
void
modify_ticket_element_start (gmp_parser_t *gmp_parser, const gchar *name,
                             const gchar **attribute_names,
                             const gchar **attribute_values)
{
  //element_start (&spec, modify_ticket_data.context...);
  xml_handle_start_element (modify_ticket_data.context, name, attribute_names,
                            attribute_values);
}

/**
 * @brief Execute command.
 *
 * @param[in]  gmp_parser   GMP parser.
 * @param[in]  error        Error parameter.
 */
void
modify_ticket_run (gmp_parser_t *gmp_parser, GError **error)
{
  entity_t entity, comment, status, solved_comment, closed_comment;
  entity_t assigned_to;
  const char *ticket_id, *user_id;

  entity = (entity_t) modify_ticket_data.context->first->data;

  ticket_id = entity_attribute (entity, "ticket_id");

  comment = entity_child (entity, "comment");
  status = entity_child (entity, "status");
  solved_comment = entity_child (entity, "solved_comment");
  closed_comment = entity_child (entity, "closed_comment");

  assigned_to = entity_child (entity, "assigned_to");
  if (assigned_to)
    {
      entity_t user;

      user = entity_child (assigned_to, "user");
      if (user == NULL)
        {
          SEND_TO_CLIENT_OR_FAIL
           (XML_ERROR_SYNTAX ("modify_ticket",
                              "MODIFY_TICKET requires USER in ASSIGNED_TO"));
          modify_ticket_reset ();
          return;
        }

      user_id = entity_attribute (user, "id");
      if ((user_id == NULL) || (strlen (user_id) == 0))
        {
          SEND_TO_CLIENT_OR_FAIL
           (XML_ERROR_SYNTAX ("modify_ticket",
                              "MODIFY_TICKET USER must have an id"
                              " attribute"));
          modify_ticket_reset ();
          return;
        }
    }
  else
    user_id = NULL;

  if (ticket_id == NULL)
    SEND_TO_CLIENT_OR_FAIL
     (XML_ERROR_SYNTAX ("modify_ticket",
                        "MODIFY_TICKET requires a ticket_id"
                        " attribute"));
  else switch (modify_ticket
                (ticket_id,
                 comment ? entity_text (comment) : NULL,
                 status ? entity_text (status) : NULL,
                 solved_comment ? entity_text (solved_comment) : NULL,
                 closed_comment ? entity_text (closed_comment) : NULL,
                 user_id))
    {
      case 0:
        SENDF_TO_CLIENT_OR_FAIL (XML_OK ("modify_ticket"));
        log_event ("ticket", "Ticket", ticket_id, "modified");
        break;
      case 1:
        SEND_TO_CLIENT_OR_FAIL
         (XML_ERROR_SYNTAX ("modify_ticket",
                            "Ticket exists already"));
        log_event_fail ("ticket", "Ticket", ticket_id, "modified");
        break;
      case 2:
        log_event_fail ("ticket", "Ticket", ticket_id, "modified");
        if (send_find_error_to_client ("modify_ticket", "ticket", ticket_id,
                                       gmp_parser))
          {
            error_send_to_client (error);
            return;
          }
        break;
      case 3:
        log_event_fail ("ticket", "Ticket", ticket_id, "modified");
        if (send_find_error_to_client ("modify_ticket", "user", user_id,
                                       gmp_parser))
          {
            error_send_to_client (error);
            return;
          }
        break;
      case 4:
        SEND_TO_CLIENT_OR_FAIL
         (XML_ERROR_SYNTAX ("modify_ticket",
                            "Error in status"));
        log_event_fail ("ticket", "Ticket", ticket_id, "modified");
        break;
      case 5:
        SEND_TO_CLIENT_OR_FAIL
         (XML_ERROR_SYNTAX ("modify_ticket",
                            "Solved STATUS requires a SOLVED_COMMENT"));
        log_event_fail ("ticket", "Ticket", ticket_id, "modified");
        break;
      case 6:
        SEND_TO_CLIENT_OR_FAIL
         (XML_ERROR_SYNTAX ("modify_ticket",
                            "Closed STATUS requires a CLOSED_COMMENT"));
        log_event_fail ("ticket", "Ticket", ticket_id, "modified");
        break;
      case 99:
        SEND_TO_CLIENT_OR_FAIL
         (XML_ERROR_SYNTAX ("modify_ticket",
                            "Permission denied"));
        log_event_fail ("ticket", "Ticket", ticket_id, "modified");
        break;
      case -1:
      default:
        SEND_TO_CLIENT_OR_FAIL
         (XML_INTERNAL_ERROR ("modify_ticket"));
        log_event_fail ("ticket", "Ticket", ticket_id, "modified");
        break;
    }

  modify_ticket_reset ();
}

/**
 * @brief End element.
 *
 * @param[in]  gmp_parser   GMP parser.
 * @param[in]  error        Error parameter.
 * @param[in]  name         Element name.
 *
 * @return 0 success, 1 command finished.
 */
int
modify_ticket_element_end (gmp_parser_t *gmp_parser, GError **error,
                           const gchar *name)
{
  //element_end (&spec, modify_ticket_data.context...);
  xml_handle_end_element (modify_ticket_data.context, name);
  if (modify_ticket_data.context->done)
    {
      modify_ticket_run (gmp_parser, error);
      return 1;
    }
  return 0;
}

/**
 * @brief Add text to element.
 *
 * @param[in]  text         Text.
 * @param[in]  text_len     Text length.
 */
void
modify_ticket_element_text (const gchar *text, gsize text_len)
{
  //element_text (&spec, modify_ticket_data.context...);
  xml_handle_text (modify_ticket_data.context, text, text_len);
}


/* Result ticket support. */

/**
 * @brief Buffer ticket XML for a result.
 *
 * @param[in]  buffer     Buffer.
 * @param[in]  result_id  ID of result.
 *
 * @return 0 success, -1 internal error.
 */
int
buffer_result_tickets_xml (GString *buffer, const gchar *result_id)
{
  iterator_t tickets;
  int ret;

  ret = init_result_ticket_iterator (&tickets, result_id);

  if (ret == 0)
    {
      buffer_xml_append_printf (buffer, "<tickets>");
      while (next (&tickets))
        buffer_xml_append_printf (buffer,
                                  "<ticket id=\"%s\"/>",
                                  result_ticket_iterator_ticket_id
                                   (&tickets));
      buffer_xml_append_printf (buffer, "</tickets>");
      cleanup_iterator (&tickets);
    }

  return ret;
}
