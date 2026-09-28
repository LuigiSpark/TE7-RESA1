# Répertoire de travail

Ce répertoire ne fournit aucune implémentation. Il contient uniquement les interfaces minimales proposées pour structurer le travail.

```text
travail/
├── include/
│   ├── protocol.h       # Contrat de protocole et I/O TCP
│   ├── client.h         # API du client
│   ├── server.h         # API du serveur
│   ├── user_list.h      # Liste des utilisateurs
│   ├──	channel_list.h   # Liste des salons, jalon 4 uniquement
│   └── msg_struct.h 	 # Protocole de messages
├── src/ 
    ├── protocol.c       
│   ├── client.c       
│   ├── server.c         
│   ├── user_list.c      
│   └── channel_list.c
├── Makefile
└── README.md
```

Les noms de fonctions sont des propositions de découpage, il n'est pas obligatoire de respecter ce découpage. Chaque équipe peut les faire évoluer, à condition de conserver une architecture modulaire, un `main()` court et des responsabilités séparées.

Les décisions de protocole sont décrites dans [../protocole.md](../protocole.md).
