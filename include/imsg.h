 /******************************************************************************************************************************/
 /* ABLS-AGENT-IMSG/include/imsg.h       Header de l'agent IMSG                                                             */
 /* Projet Abls-Habitat                   Gestion d'habitat                                                12.09.2026 00:00:00 */
 /* Auteur: LEFEVRE Sebastien                                                                                                  */
 /******************************************************************************************************************************/
 /*
	* imsg.h
	* This file is part of Abls-Habitat
	*
	* Copyright (C) 1988-2026 - Sebastien LEFEVRE
	*
	* ABLS-AGENT-IMSG is free software; you can redistribute it and/or modify
	* it under the terms of the GNU General Public License as published by
	* the Free Software Foundation; either version 2 of the License, or
	* (at your option) any later version.
	*/

#ifndef _ABLS_IMSG_H_
 #define _ABLS_IMSG_H_

 #include <glib.h>
 #include <strophe.h>
 #include <abls-agent-libs/abls-agent-libs.h>

 struct ABLS_IMSG_VARS
  { xmpp_ctx_t *ctx;
    xmpp_conn_t *conn;
    gboolean signed_off;
    gchar *jabber_id;
    gchar *jabber_password;
  };

 extern struct ABLS_AGENT *Agent;
 extern struct ABLS_IMSG_VARS *Agent_vars;

#endif
/*----------------------------------------------------------------------------------------------------------------------------*/
