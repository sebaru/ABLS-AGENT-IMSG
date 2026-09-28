/******************************************************************************************************************************/
/* ABLS-AGENT-IMSG/src/imsg.c          Agent de messagerie IMSG via XMPP                                                      */
/* Projet Abls-Habitat                   Gestion d'habitat                                                12.09.2026 00:00:00 */
/* Auteur: LEFEVRE Sebastien                                                                                                  */
/******************************************************************************************************************************/
/*
 * imsg.c
 * This file is part of Abls-Habitat
 *
 * Copyright (C) 1988-2026 - Sebastien LEFEVRE
 *
 * ABLS-AGENT-IMSG is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ABLS-AGENT-IMSG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

 #include "imsg.h"

 struct ABLS_AGENT *Agent = NULL;
 struct ABLS_IMSG_VARS *Agent_vars = NULL;

/******************************************************************************************************************************/
/* Imsg_send_message_to: Envoie un message XMPP à un destinataire                                                             */
/* Entrée: l'agent, le destinataire et le message                                                                             */
/* Sortie: néant                                                                                                              */
/******************************************************************************************************************************/
 static void Imsg_send_message_to ( const gchar *dest, const gchar *message )
  { xmpp_stanza_t *stanza = xmpp_message_new ( Agent_vars->ctx, "normal", dest, NULL );
    xmpp_message_set_body ( stanza, message );
    xmpp_send ( Agent_vars->conn, stanza );
    xmpp_stanza_release ( stanza );
    Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_INFO, "Send '%s' to '%s'", message, dest );
  }
 /******************************************************************************************************************************/
 /* Imsg_send_message_to_all_available: Envoie un message aux utilisateurs disponibles                                         */
 /* Entrée: l'agent et le message                                                                                              */
 /* Sortie: néant                                                                                                              */
 /******************************************************************************************************************************/
 static void Imsg_send_message_to_all_available ( const gchar *message )
  { JsonNode *UsersNode = Http_Get_from_global_API ( Agent, "/run/users/wanna_be_notified", NULL );
    if (!UsersNode || Json_get_int ( UsersNode, "http_code" ) != 200)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR, "Could not get USERS from API" );
       if (UsersNode) Json_unref ( UsersNode );
       return;
     }

    GList *Recipients = json_array_get_elements ( Json_get_array ( UsersNode, "recipients" ) );
    GList *recipients = Recipients;
    while(recipients)
     { JsonNode *user = recipients->data;
       gchar *xmpp = Json_get_string ( user, "xmpp" );
       if (!xmpp)
        { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR,
                    "User %s does not have an XMPP id", Json_get_string ( user, "email" ) );
        }
       else if (!strlen(xmpp))
        { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR,
                    "User %s has an empty XMPP id", Json_get_string ( user, "email" ) );
        }
       else Imsg_send_message_to ( xmpp, message );
       recipients = g_list_next(recipients);
     }
    g_list_free(Recipients);
    Json_unref ( UsersNode );
  }
 /******************************************************************************************************************************/
 /* Imsg_handle_message_CB: Traite un message XMPP reçu et ses droits d'exécution                                             */
 /* Entrée: la connexion, la stanza et l'agent                                                                                 */
 /* Sortie: 1 pour conserver le callback actif                                                                                 */
 /******************************************************************************************************************************/
 static int Imsg_handle_message_CB ( xmpp_conn_t *const conn, xmpp_stanza_t *const stanza, void *const userdata )
  { const gchar *from = xmpp_stanza_get_attribute ( stanza, "from" );
    if (!from)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR, "Missing from" );
       return(1);
     }

    const gchar *type = xmpp_stanza_get_type ( stanza );
    if (type && !strcmp ( type, "error" ))
     { xmpp_stanza_t *error = xmpp_stanza_get_child_by_name(stanza, "error");
       const gchar *error_type = xmpp_stanza_get_attribute ( error, "type" );
       xmpp_stanza_t *condition = xmpp_stanza_get_children ( error );
       const gchar *condition_name = xmpp_stanza_get_name ( condition );
       Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR,
             "From '%s' -> Stanza Error '%s'->'%s'", from, error_type, condition_name );
       return(1);
     }

    gchar *message = xmpp_message_get_body ( stanza );
    if (!message)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR, "From '%s' -> missing message", from );
       gchar *buf; size_t buflen;
       xmpp_stanza_to_text ( stanza, &buf, &buflen );
       Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR, "Received stanza '%s'", buf );
       xmpp_free(Agent_vars->ctx, buf);
       return(1);
     }

    Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE, "From '%s' -> '%s'", from, message );

    JsonNode *RootNode = Json_create();
    if (RootNode == NULL)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ALERT, "Memory Error for '%s'", from );
       goto end_message;
     }

    gchar hostonly[128];
    g_snprintf( hostonly, sizeof(hostonly), "%s", from );
    gchar *ptr = strstr( hostonly, "/" );
    if (ptr) *ptr = 0;

    Json_add_string ( RootNode, "xmpp", hostonly );

    JsonNode *UserNode = Http_Post_to_global_API ( Agent, "/run/user/can_send_txt_cde", RootNode );
    Json_unref ( RootNode );
    if (!UserNode || Json_get_int ( UserNode, "http_code" ) != 200)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR, "Could not get USER from API for '%s'", from );
       goto end_user;
     }

    if (!Json_has_member ( UserNode, "email" ))
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR,
             "%s is not a known user. Dropping command '%s'...", from, message );
       goto end_user;
     }

    if (!Json_has_member ( UserNode, "can_send_txt_cde" ) || Json_get_bool ( UserNode, "can_send_txt_cde" ) == FALSE)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_WARNING,
             "%s ('%s') is not allowed to send txt_cde. Dropping command '%s'...",
             from, Json_get_string ( UserNode, "email" ), message );
       goto end_user;
     }

    if (!strcasecmp( message, "ping" ))
     { Imsg_send_message_to ( from, "Pong !" );
       goto end_user;
     }

    RootNode = Json_create();
    if (RootNode == NULL)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR, "MapNode Error for '%s'", from );
       goto end_user;
     }
    Json_add_string ( RootNode, "agent_tech_id", "_COMMAND_TEXT" );
    Json_add_string ( RootNode, "agent_acronyme", message );

    JsonNode *MapNode = Http_Post_to_global_API ( Agent, "/run/mapping/search_txt", RootNode );
    Json_unref ( RootNode );
    if (!MapNode || Json_get_int ( MapNode, "http_code" ) != 200)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR, "Could not search mapping for '%s'", message );
       goto end_map;
     }

    if (Json_has_member ( MapNode, "nbr_results" ) == FALSE)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR, "Error searching Database for user '%s'", from );
       Imsg_send_message_to ( from, "Error searching Database .. Sorry .." );
       goto end_map;
     }

    gint nbr_results = Json_get_int ( MapNode, "nbr_results" );
    if (nbr_results == 0)
     { Imsg_send_message_to ( from, "Je n'ai pas trouvé, désolé." ); }
    else
     { if (nbr_results > 1)
        { Imsg_send_message_to ( from, "Aîe, plusieurs choix sont possibles ... :" ); }

       GList *Results = json_array_get_elements ( Json_get_array ( MapNode, "results" ) );
       if (nbr_results > 1)
        { GList *results = Results;
          while(results)
           { JsonNode *element = results->data;
             gchar *thread_acronyme = Json_get_string ( element, "thread_acronyme" );
             Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_INFO,
                   "Map found for '%s' -> '%s'", from, thread_acronyme );
             Imsg_send_message_to ( from, thread_acronyme );
             results = g_list_next(results);
           }
        }
       else if (nbr_results == 1)
        { JsonNode *element = Results->data;
          gchar *tech_id = Json_get_string ( element, "tech_id" );
          gchar *acronyme = Json_get_string ( element, "acronyme" );
          Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_INFO,
                "Map found for '%s' -> '%s:%s'", from, tech_id, acronyme );
          Mqtt_Send_DI_pulse ( Agent, tech_id, acronyme );
          gchar chaine[256];
          g_snprintf ( chaine, sizeof(chaine), "'%s' fait.", message );
          Imsg_send_message_to ( from, chaine );
        }
       g_list_free(Results);
     }

end_map:
    Json_unref ( MapNode );
end_user:
    Json_unref ( UserNode );
end_message:
    xmpp_free(Agent_vars->ctx, message);
    return(1);
  }

 /******************************************************************************************************************************/
 /* Imsg_set_presence: Publie la présence XMPP de l'agent                                                                       */
 /* Entrée: l'agent et le texte de présence                                                                                     */
 /* Sortie: néant                                                                                                              */
 /******************************************************************************************************************************/
 static void Imsg_set_presence ( const char *status_to_send )
  { xmpp_stanza_t *pres = xmpp_presence_new(Agent_vars->ctx);

    xmpp_stanza_t *show = xmpp_stanza_new(Agent_vars->ctx);
    xmpp_stanza_set_name ( show, "show" );
    xmpp_stanza_t *show_text = xmpp_stanza_new(Agent_vars->ctx);
    xmpp_stanza_set_text ( show_text, "chat" );
    xmpp_stanza_add_child ( show, show_text );
    xmpp_stanza_add_child ( pres, show );

    xmpp_stanza_t *status = xmpp_stanza_new(Agent_vars->ctx);
    xmpp_stanza_set_name ( status, "status" );
    xmpp_stanza_t *status_text = xmpp_stanza_new(Agent_vars->ctx);
    xmpp_stanza_set_text ( status_text, status_to_send );
    xmpp_stanza_add_child ( status, status_text );
    xmpp_stanza_add_child ( pres, status );

    xmpp_send(Agent_vars->conn, pres);
    xmpp_stanza_release(pres);
  }
 /******************************************************************************************************************************/
 /* Imsg_handle_presence_CB: Accepte les demandes d'abonnement XMPP                                                            */
 /* Entrée: la connexion, la stanza et l'agent                                                                                 */
 /* Sortie: 1 pour conserver le callback actif                                                                                 */
 /******************************************************************************************************************************/
 static int Imsg_handle_presence_CB ( xmpp_conn_t *const conn, xmpp_stanza_t *const stanza, void *const userdata )
  { const char *type, *from;
    type = xmpp_stanza_get_type ( stanza );
    from = xmpp_stanza_get_from ( stanza );

    if (type && !strcmp(type,"subscribe"))
     { xmpp_stanza_t *pres;
       pres = xmpp_presence_new(Agent_vars->ctx);
       xmpp_stanza_set_to ( pres, from );
       xmpp_stanza_set_type( pres, "subscribed" );
       xmpp_send(Agent_vars->conn, pres);
       xmpp_stanza_release(pres);
       pres = xmpp_presence_new(Agent_vars->ctx);
       xmpp_stanza_set_to ( pres, from );
       xmpp_stanza_set_type( pres, "subscribe" );
       xmpp_send(Agent_vars->conn, pres);
       xmpp_stanza_release(pres);
       Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE, "Sending 'subscribe' to '%s'", from );
     }
    return(1);
  }
 /******************************************************************************************************************************/
 /* Imsg_connexion_CB: Enregistre les callbacks et l'état de la connexion XMPP                                                  */
 /* Entrée: la connexion, son état, l'erreur éventuelle et l'agent                                                              */
 /* Sortie: néant                                                                                                              */
 /******************************************************************************************************************************/
 static void Imsg_connexion_CB ( xmpp_conn_t *const conn, const xmpp_conn_event_t status, const int error,
                                 xmpp_stream_error_t *const stream_error, void *const userdata )
  { if (status == XMPP_CONN_CONNECT)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE,
             "Account connected and %s secure", (xmpp_conn_is_secured (conn) ? "IS" : "IS NOT") );
       xmpp_handler_add ( Agent_vars->conn, Imsg_handle_message_CB, NULL, "message", NULL, NULL );
       xmpp_handler_add ( Agent_vars->conn, Imsg_handle_presence_CB, NULL, "presence", NULL, NULL );
       Imsg_set_presence ( "A votre écoute !" );
       Imsg_send_message_to_all_available ( "Agent démarré. A l'écoute !" );
     }
    else
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE, "Account disconnected" );
       Agent_vars->signed_off = TRUE;
     }
  }

 /******************************************************************************************************************************/
 /* main: Initialise l'agent IMSG, traite les messages puis libère XMPP                                                        */
 /* Entrée: argc et argv                                                                                                       */
 /* Sortie: code de retour du programme                                                                                        */
 /******************************************************************************************************************************/
 gint main ( gint argc, gchar *argv[] )
  { Config_add_parameter ( "jabber_id", "JABBER_ID", "XMPP JID", CONFIG_STRING );
    Config_add_parameter ( "jabber_password", "PASS", "XMPP password", CONFIG_STRING );

    Agent = Agent_init ( argv[0], "imsg", ABLS_AGENT_IMSG_VERSION, sizeof(struct ABLS_IMSG_VARS), argc, argv );
    Agent_vars = Agent_get_vars ( Agent );

    Agent_vars->jabber_id       = Agent_config_get_string ( Agent, "jabber_id" );
    Agent_vars->jabber_password = Agent_config_get_string ( Agent, "jabber_password" );

    if (!Agent_vars->jabber_id || !Agent_vars->jabber_password)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR, "Missing jabber_id or password, stopping." );
       Agent_end ( Agent );
     }

    Agent_subscribe_mqtt_local ( Agent, "SEND_IMSG" );

    Agent_is_ready ( Agent );

    while (Agent_is_running ( Agent ))
     { Agent_loop ( Agent );
/****************************************************** Ecoute du master ******************************************************/
       JsonNode *mqtt_local_message;
       while ( (mqtt_local_message = Agent_get_mqtt_local_message ( Agent )) != NULL )
        { if (Json_has_member ( mqtt_local_message, "token_lvl0" ))
           { gchar *token_lvl0 = Json_get_string ( mqtt_local_message, "token_lvl0" );
             if (!strcasecmp (token_lvl0, "SEND_IMSG") &&
                 Json_has_member ( mqtt_local_message, "tech_id" ) && Json_has_member ( mqtt_local_message, "acronyme" ) &&
                 Json_has_member ( mqtt_local_message, "libelle" ))
              { char chaine[256];
                g_snprintf ( chaine, sizeof(chaine), "%s: %s", Json_get_string ( mqtt_local_message, "dls_shortname" ), Json_get_string ( mqtt_local_message, "libelle" ) );
                Imsg_send_message_to_all_available ( chaine );
              }
             else if (!strcasecmp (token_lvl0, "SET_TEST"))
              { Imsg_send_message_to_all_available ( "Test OK" ); }
           }
          Json_unref ( mqtt_local_message );
        }

/****************************************************** Ecoute de l'api *******************************************************/
       JsonNode *mqtt_api_message;
       while ( (mqtt_api_message = Agent_get_mqtt_api_message ( Agent ) ) != NULL )
        { if ( Mqtt_topic_is ( mqtt_api_message, 4, "+", "AGENT", Agent_get_tech_id ( Agent ), "TEST" ) )
           { Info(__func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE, "Agent Test from API.");
             Imsg_send_message_to_all_available ( "Test OK" );
           }
          Json_unref (mqtt_api_message);
        }

/****************************************************** Fonctionnel ***********************************************************/
       if (!Agent_vars->ctx || !Agent_vars->conn || Agent_vars->signed_off)
        { Agent_vars->signed_off = FALSE;
          Agent_vars->ctx = xmpp_ctx_new(NULL, xmpp_get_default_logger(XMPP_LEVEL_INFO));
          if (!Agent_vars->ctx)
           { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR, "Ctx Init failed" );
             sleep(2); continue;
           }
          Agent_vars->conn = xmpp_conn_new(Agent_vars->ctx);
          if (!Agent_vars->conn)
           { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR, "Connection New failed" );
             xmpp_ctx_free(Agent_vars->ctx); Agent_vars->ctx = NULL; sleep(2); continue;
           }
          xmpp_conn_set_sockopt_callback(Agent_vars->conn, xmpp_sockopt_cb_keepalive);
          xmpp_conn_set_jid (Agent_vars->conn, Agent_vars->jabber_id);
          xmpp_conn_set_pass(Agent_vars->conn, Agent_vars->jabber_password);
          gint retour = xmpp_connect_client ( Agent_vars->conn, NULL, 0, Imsg_connexion_CB, Agent );
          if (retour != XMPP_EOK)
           { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR,
                   "Connexion failed with error %d", retour );
             Agent_vars->signed_off = TRUE;
             xmpp_disconnect(Agent_vars->conn);
             xmpp_conn_release(Agent_vars->conn);
             Agent_vars->conn = NULL;
             xmpp_ctx_free(Agent_vars->ctx); Agent_vars->ctx = NULL;
             sleep(2);
           }
        }

       if (Agent_vars->ctx && Agent_vars->conn && !Agent_vars->signed_off)
        { xmpp_run_once ( Agent_vars->ctx, 500 ); }
     }

    if (Agent_vars->conn)
     { xmpp_disconnect(Agent_vars->conn); xmpp_conn_release(Agent_vars->conn); Agent_vars->conn = NULL; }
    if (Agent_vars->ctx)
     { xmpp_ctx_free(Agent_vars->ctx); Agent_vars->ctx = NULL; }

    Agent_end ( Agent );
  }
/*----------------------------------------------------------------------------------------------------------------------------*/
