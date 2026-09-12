# abls-agent-imsg

Runtime XMPP/IMSG pour Abls-Habitat, migré du module historique `Watchdogd/Imsgs` vers le modèle agent ABLS.

## État

- squelette d’agent ABLS prêt
- logique XMPP migrée depuis le module legacy
- build et packaging alignés sur les autres agents ABLS
- sans Containerfile

## Build

```sh
./install_deps.sh
./build.sh
```

## Packaging RPM

```sh
./build_rpm.sh
```

## Packaging DEB

```sh
./build_apt.sh --dist bookworm --no-sign
./build_apt.sh --dist trixie --no-sign
```

## Release

```sh
./bump.sh 1.0.0
```

## Configuration

Le runtime attend les paramètres standard ABLS d’un agent, ainsi que les éléments suivants :

- `jabber_id` ou `jabberid`
- `jabber_password` ou `password`
- `agent_tech_id`
- `api_url`, `domain_uuid`, `domain_secret`, `server_uuid`

## Fonctionnement

L’agent connecte un compte XMPP via libstrophe, publie son statut, et écoute les messages entrants pour vérifier l’utilisateur, rechercher un mapping texte, et envoyer les commandes associées au domaine.
