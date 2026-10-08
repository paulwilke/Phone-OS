# Guide de l'utilisateur

Pour les parents. Ce guide est écrit à partir du code et des autres documents de ce dépôt ; les touches et les pages portent le nom que leur donne le catalogue français (`lang/fr.toml`). Il dit ce que fait le logiciel. Ce qui a été confirmé sur l'appareil réel n'y figure pas : voir [bring-up.md](../bring-up.md) et [hardware-lessons.md](../hardware-lessons.md) (en anglais). Le guide existe aussi en anglais ([user-guide.md](../user-guide.md)) et en allemand ([de/benutzerhandbuch.md](../de/benutzerhandbuch.md)), avec la même structure.

## Ce qu'est l'appareil

Un téléphone d'urgence pour un enfant, sur le LilyGO T-Deck Pro MAX : un petit écran noir et blanc qui garde son image sans courant, un clavier, trois touches sous l'écran, une carte SIM pour les appels et les textos, un récepteur de satellites (GPS) et une carte avec les réglages. Il fait peu de choses et rien d'autre : l'enfant appelle et écrit aux personnes que les parents ont autorisées, envoie une urgence avec sa position, et l'appareil se tait pendant les heures de classe. Les parents le règlent depuis leur propre téléphone, dans un navigateur.

L'écran d'accueil de l'enfant est une grille de tuiles, trois par rangée, sur deux pages. La première page est celle de l'enfant : **Appels**, **Écrire**, **SOS**, **Courrier**, si les parents ont autorisé la radio **Radio**, et **Lumière**. La seconde page est celle de l'appareil : **Réseau**, **GPS**, **Heure**, **Options**, **Mouv.** (si l'appareil a un capteur de mouvement) et **Tests**, et, seulement si les parents ont activé les outils des parents dans les réglages, **Wi-Fi** et **État**. Les parents peuvent masquer chaque tuile de la seconde page (`[home]` dans `settings.toml`, onglet **Avancé** de la page des parents) ; sans la tuile **Wi-Fi**, la page des parents reste accessible par le câble USB. Un contact sur la moitié gauche ou droite de la bande sous les tuiles montre la première ou la seconde page ; les touches ▲ et ▼ parcourent toutes les tuiles. Une tuile sans objet n'est pas là : sans personne à appeler, il n'y a pas de tuiles **Appels**, **Écrire** et **SOS**.

## Ce que l'enfant peut faire

- **Appels** : choisir qui appeler dans la liste faite par les parents. L'appareil ne compose que cette liste. Un appel se termine avec **Raccrocher**, toute la barre sous l'écran ; pendant l'appel, l'enfant règle le **Volume** avec les boutons **Moins fort** et **Plus fort** à l'écran ou les touches ▲ et ▼.
- **Écrire** : choisir à qui écrire, puis envoyer l'un des six messages tout prêts ("Je suis en route", "Je suis arrivé", "Peux-tu m'appeler ?", "Je serai en retard", "OK", "Merci"). Ils s'affichent dans la langue de l'enfant et partent dans la langue que les parents ont choisie pour les textos. Si les parents ont activé le texte libre (`free_text`), l'enfant tape tout de suite n'importe quel texte et peut choisir **Autres** et **Nouveau numéro** ; un texte tapé compte 160 caractères, 70 seulement avec des accents. Les messages tout prêts restent à une touche : tant que rien n'est tapé, la touche de droite dit **Modèles**.
- **Courrier** : les messages reçus et envoyés (la touche O passe de **Réception** à **Envoyés**, X efface un message), avec **Répondre** et **Rappeler**. Un message qui n'est pas encore parti porte "(en attente)" ; un message arrivé, "arrivé".
- **Lumière** : la lumière frontale de l'écran, allumée et éteinte.
- **Retour** sur l'écran d'accueil verrouille l'appareil. Pour déverrouiller, appuyer deux fois de suite sur la touche Entrée (↵) : l'écran dit "Appuyez deux fois sur ↵ pour déverrouiller". Un contact avec l'écran et les trois touches en dessous ne déverrouillent jamais, une poche ne peut donc pas le faire. L'appareil se verrouille aussi tout seul après 120 secondes sans action, sauf pendant un appel. L'écran verrouillé montre à quoi l'appareil est connecté et "2 nouveaux messages" quand quelque chose n'est pas lu.

## Qui peut appeler et écrire : le carnet de contacts

**Seules les personnes du carnet de contacts peuvent écrire à l'appareil et l'appeler. Personne d'autre.** Le carnet de contacts est la liste des contacts (`contacts.toml`, l'onglet **Contacts** de la page des parents) avec les numéros des boutons d'appel. Il n'y a pas d'interrupteur. Un texto ou un appel de quelqu'un d'autre est écarté sans que l'enfant le sache : pas de sonnerie, pas de vibration, pas d'avis ; une ligne du journal dit que quelqu'un a frappé.

Les limites de cette règle, pour que personne ne lui fasse trop confiance :

- **Un numéro peut être falsifié.** L'appareil croit le numéro d'expéditeur que le réseau transmet. Qui connaît un numéro de la famille peut envoyer depuis celui-ci. La règle écarte la publicité, les appels commerciaux, les inconnus et les inondations ; elle ne protège pas contre quelqu'un qui connaît la famille et falsifie un numéro.
- **Les textos de l'opérateur sont aussi écartés** (solde, tarif, avis de PIN). Ils ne sont pas perdus : le journal porte les 120 premiers caractères du premier texto écarté d'un expéditeur en dix minutes. Lisez-les dans l'onglet **Journal** de la page des parents. Un texte écarté peut donc se retrouver dans le journal, et qui a le mot de passe web peut lire le journal.
- **Un membre de la famille qui masque son numéro est refusé** comme tout appel masqué.
- Une inondation ne remplit pas le journal : une ligne par expéditeur en dix minutes, puis une ligne qui compte le reste.
- La radio est une voie à part avec sa propre règle ; elle est éteinte tant que les parents ne l'ont pas activée.

Un contact a un rôle : **parent** ou **family** (famille) (`role`). Un parent peut aussi demander sa position à l'appareil par texto (ci-dessous) ; la famille peut appeler et écrire.

## Heures de classe

Les heures de classe sont un ensemble de plages (`[[quiet_hours]]` dans `settings.toml` : `days`, `from`, `to`, `enabled` ; dans l'onglet **Heures de classe**, chaque plage est un cadre : les jours se cochent (lu à di), en dessous de, à et "actif" ; **Ajouter** ajoute une plage, par exemple lundi et mardi après-midi ; par exemple `mon-fri` de `08:00` à `13:30` ; au plus deux plages par jour, une plage peut passer minuit). Une plage ne compte que si elle est **active** : sur la page des parents, chaque plage a une case "actif" (`enabled = true` ou `false` ; une plage sans cette clé est active). Un appareil sans aucune plage démarre avec une plage intégrée, du lundi au vendredi de 08:00 à 12:30, **non active** : aucune heure de classe ne court tant qu'un parent n'a pas coché la case et enregistré. Tant qu'une plage active court :

- la barre d'état dit "En classe", et l'écran verrouillé dit "Classe jusqu'à 13:30" ;
- l'enfant ne peut pas appeler : la tuile **Appels** dit "Classe jusqu'à 13:30" et reste là ;
- **tout appel entrant d'un contact est refusé sans sonner, parents compris.** L'enfant voit "(nom) essaie d'appeler", et l'appel figure dans le courrier comme "Appel manqué de (nom)", pour qu'un parent puisse rappeler ;
- les textos arrivent toujours et sont gardés ; seul un texto d'un parent fait vibrer l'appareil ;
- **SOS** fonctionne toujours, mais demande cinq frappes (ci-dessous).

Sans heure connue, il n'y a pas d'heures de classe : l'appareil n'a pas d'horloge propre et prend l'heure du réseau. Un appareil qui vient de démarrer sans réseau ne verrouille pas.

## SOS

Appuyer sur la tuile **SOS**. L'appareil commence aussitôt à chercher la position, pour qu'elle soit prête quand l'enfant aura décidé. L'urgence va à chaque contact coché **SOS** dans l'onglet **Contacts** (`sos = true` dans `contacts.toml`), à tous avec le même texte ; sans coche, elle va au numéro du premier bouton d'appel (`[[tiles]]` avec `action = "call"`). Elle compte comme envoyée dès qu'un texte est parti ; si le réseau refuse l'un de plusieurs textes, l'enfant n'en sait rien, le journal le dit.

Hors des heures de classe, la page offre trois choix : "Urgence ! Je suis ici", "Viens me chercher" et "Annuler". Si l'enfant ne choisit rien, la page compte "Urgence dans 40 s" puis envoie l'urgence toute seule. **Retour** ou **Annuler** n'envoie rien. Le texte porte le nom de l'enfant, un lien de carte et les coordonnées avec leur précision ("Position précise à 12 m") ; sans réception satellite, il le dit ("Sans position : pas de GPS") et part quand même après le temps de recherche. La page montre les étapes "Localisation", "Position trouvée", "Envoyé" et à qui ("À (nom)", ou "attend le réseau"). S'il n'y a ni réseau ni radio, elle dit "Non envoyé" et propose "Réessayer".

Pendant les heures de classe, la page dit "Classe jusqu'à 13:30. L'appareil est verrouillé." et "Une vraie urgence ? Tapez 5 fois, chaque fois dans les 10 s après la précédente." La cinquième frappe, chacune dans les dix secondes après la précédente, envoie l'urgence et lève le verrouillage jusqu'à la fin de ces heures de classe (pour que l'enfant puisse appeler et être appelé). Quatre frappes ne font rien ; une pause de dix secondes recommence le compte. La levée est écrite dans le journal.

Un parent peut aussi demander où est l'enfant : un texto qui ne contient que `#OuEsTu` (ou `#WhereAreYou`, `#WoBistDu` ; les majuscules ne comptent pas), envoyé depuis le numéro d'un parent, fait répondre l'appareil avec sa position. Un inconnu n'a pas de réponse et ne sait pas que la commande existe. Sur la page **État**, la ligne "Requêtes à distance" dit à quel point c'est visible pour l'enfant : "silencieuses", "dans l'historique" ou "avec notification" ; la touche Entrée (touche programmable "Requêtes") fait défiler les trois.

## La page des parents

La page des parents est une page web que l'appareil sert par Wi-Fi. Elle est pour le téléphone ou l'ordinateur des parents, jamais pour l'enfant.

1. **Activer le Wi-Fi.** La tuile **Wi-Fi** est sur la seconde page de l'accueil et n'existe que si les outils des parents sont activés (`parent_tools = true` dans `settings.toml`). Sur la page **Wi-Fi**, choisir **Point d'accès** (le réseau propre de l'appareil, pour un endroit sans réseau maison) ou **Réseau maison** (un réseau enregistré sur la carte) ; **Désactivé** l'éteint. La page montre alors "Réseau : ...", le "Mot de passe : ..." du point d'accès (un nouveau mot de passe aléatoire à chaque démarrage, tant qu'aucun n'est fixé dans `secrets.toml` ; il n'apparaît que sur l'écran de l'appareil) et "Page : http://(adresse)". Si la batterie est trop faible pour le Wi-Fi, l'appareil refuse et dit "Batterie faible : pas de Wi-Fi".
2. **Se connecter et ouvrir l'adresse** sur le téléphone des parents, dans un navigateur.
3. **Se connecter** avec le mot de passe web (**Mot de passe web** dans le fichier `secrets.toml`, au moins 4 caractères). Sans mot de passe enregistré, la page reste verrouillée : mettez-en d'abord un dans `secrets.toml` sur la carte. Cinq mots de passe faux verrouillent la page pendant soixante secondes. La page parle HTTP sans chiffrement, le mot de passe et les secrets traversent donc le Wi-Fi en clair : utilisez-la sur le point d'accès propre de l'appareil ou sur un réseau de confiance.
4. **Les onglets :** **État**, **Général**, **Heures de classe**, **Avancé**, **Contacts**, **Accès**, **Fichiers**, **Journal**, **Sauvegarde**. Les réglages sont sur trois onglets : **Général** (le nom de l'enfant, les langues, les fonctions, les requêtes, les boutons d'appel), **Heures de classe** (les plages) et **Avancé** (fuseau horaire, niveau du journal, temps d'inactivité du Wi-Fi, batterie, modem, lien de carte, radio ; la plupart des parents les laissent tels quels). Les paramètres et les contacts sont des formulaires ; modifiez-les et appuyez sur **Enregistrer** dans cet onglet ; la page répond "Enregistré." et dit "Modifications non enregistrées" tant qu'une modification n'est pas enregistrée. Une modification non enregistrée reste quand vous regardez un autre onglet et revenez. Enregistrer un onglet garde ce que les autres contiennent. Un fichier que l'appareil refuserait au démarrage n'est jamais enregistré. La plupart des réglages jouent après un redémarrage : appuyez sur **Redémarrer** dans l'onglet **État**. Le vibreur des messages et la visibilité des requêtes jouent tout de suite.
5. **Garder la page ouverte :** le Wi-Fi s'éteint tout seul après 10 minutes sans usage (`auto_off_minutes`) ; chaque requête avec le mot de passe relance le temps, et l'onglet **État** montre "Le Wi-Fi s'éteint dans (n) min" et a le bouton **Garder le Wi-Fi allumé**. **Éteindre le Wi-Fi** y met fin tout de suite.
6. **Le journal :** l'onglet **Journal** (**Actualiser**, **Effacer le journal**) montre ce que l'appareil a fait : appels refusés, textos écartés, modifications faites sur la page (chemin et taille seulement, jamais le contenu), la levée des heures de classe après un SOS. Le niveau est `[log] level`.

## Les réglages

Sur l'appareil, la page **Options** (seconde page de l'accueil, tuile **Options**) a cinq cases ; la touche Entrée ou un contact change la case qui a le focus : **Volume en appel** ("4 sur 7"), **Vibreur des messages**, **Wi-Fi** (ouvre la page Wi-Fi), **Langue** (ouvre la page des langues, chacune sous son propre nom) et **Éteindre** : appuyer deux fois ("appuyer deux fois", puis "encore une fois : éteint") ; toute autre touche annule le premier appui, et avec le câble USB branché elle dit "débrancher l'USB".

Sur la page **État**, la touche Entrée fait défiler la visibilité des requêtes à distance, et "Éteindre : appuyer deux fois sur x" éteint l'appareil. Avec le câble USB branché, c'est impossible, et il dit "Éteindre : débrancher l'USB d'abord". "Redémarrer : appuyer deux fois sur r" redémarre l'appareil (encore un r à "Redémarrer ? Encore une fois r") ; toute autre touche annule le premier appui, et le redémarrage fonctionne aussi avec le câble USB branché.

Le reste est sur la page des parents ou dans les trois fichiers de la carte. Les clés dont la plupart des parents ont besoin :

| Fichier | Clé | Effet |
|---|---|---|
| `settings.toml` | `[child] name` | le nom de l'enfant dans les textos et sur la page d'état |
| `settings.toml` | `[interface] language` | la langue de l'appareil |
| `settings.toml` | `[sms] language` | la langue des textos que l'appareil envoie |
| `settings.toml` | `[features] free_text`, `radio`, `parent_tools` | texte libre et destinataire quelconque ; la radio ; les tuiles **Wi-Fi** et **État** |
| `settings.toml` | `[[quiet_hours]]` | les plages des heures de classe |
| `settings.toml` | `[[tiles]]` | les boutons d'appel ; sans contact coché SOS, le premier est le destinataire du SOS |
| `settings.toml` | `[time] timezone` | le fuseau horaire, pour les heures de classe et l'horloge |
| `settings.toml` | `[battery] capacity_mah` | la taille de la batterie, 1500 par défaut |
| `contacts.toml` | `[[contact]] number`, `name`, `role`, `sos` | qui peut appeler et écrire ; qui reçoit le SOS |
| `secrets.toml` | `sim_pin`, `web_password` | le PIN de la SIM et le mot de passe de la page des parents |

Chaque clé est listée dans [configuration-reference.md](../configuration-reference.md) (en anglais). Sur la page **État**, "Configuration : 1 problème" et le premier ("settings.toml : vérifier child.name") disent que quelque chose sur la carte n'a pas été compris ; la liste complète est dans le journal. Ce qui n'a pas été compris est ignoré et la valeur par défaut s'applique.

## Langues et comment en ajouter une

L'anglais, l'allemand et le français sont dans l'appareil. La langue de l'enfant se choisit sur la page **Langue** ou avec `[interface] language` ; la langue des textos aux parents est `[sms] language` ; la page des parents a son propre sélecteur. Une langue qui n'est pas dans l'appareil s'ajoute sans programme informatique : copier `lang/en.toml` sur la carte sous `/lang/xx.toml`, changer `[meta]` (le code, les noms), traduire les mots et garder les `{espaces réservés}` ; un fichier peut faire au plus 32 768 octets. Les mots de la page des parents vont de la même façon dans `/lang/web/xx.toml`. Puis fixer `[interface] language = "xx"` et redémarrer. Comment essayer un catalogue sans l'appareil est dans [translating.md](../translating.md) (en anglais).

## Batterie et charge

L'appareil se charge par son câble USB. La page **État** montre "Batterie : 80%" (et ", en charge" ou ", pleine sur secteur" tant qu'un câble est branché) et "Autonomie : 2 h 30 min". L'autonomie est "inconnue" tant qu'un câble est branché.

La puce de mesure de la batterie est livrée avec une mauvaise taille de batterie. À chaque démarrage, l'appareil la compare à `[battery] capacity_mah` (par défaut **1500** mAh) et n'écrit dans la puce que si elles diffèrent. Un pourcentage que la tension de la cellule contredit n'est pas montré : la page dit "Batterie : inconnue" au lieu d'un faux chiffre. Que l'écriture prenne vraiment sur la puce n'est pas confirmé sur l'appareil ([device-power.md](../device-power.md), en anglais).

## Pas de réseau, pas de SIM

- **Pas de réseau** ("Pas de réseau" sur l'écran verrouillé et sous **Réseau**) : un appel est refusé avec l'avis "Pas de réseau" ; un texto attend dans une file de huit ("Message en attente de réseau") et part quand le réseau revient ; un SOS dit "Non envoyé" et peut être réessayé, et la radio est utilisée si elle est activée.
- **Pas de SIM** (**Réseau** dit "SIM : pas de carte" et "Il n'y a pas de carte SIM dans l'appareil.") : l'appareil ne peut ni appeler ni envoyer de textos. Une SIM qui demande son PIN dit "PIN manquant" : le PIN est `sim_pin` dans `secrets.toml`, et l'appareil l'essaie une fois par démarrage. Si le PIN était faux ("PIN faux"), ne pas redémarrer avant qu'il soit juste : trois essais ratés bloquent la SIM, et seul le PUK de l'opérateur peut alors la sauver ("verrouillée").
- **SMS seulement** et "appels pas prêts" signifient que le réseau n'autorise pas les appels pour cette SIM, ou ne les a pas encore rendus prêts ; les textos fonctionnent.

## Quand l'écran reste vide

L'écran garde sa dernière image sans courant : une image qui ne change pas ne prouve donc pas que l'appareil fonctionne, et un écran vide ne signifie pas toujours qu'il est mort.

1. L'appareil peut être **en veille ou verrouillé** : appuyer sur une touche ; sur l'écran verrouillé, deux fois sur Entrée.
2. La batterie peut être **vide** : brancher le câble USB et attendre quelques minutes.
3. L'appareil peut avoir été **éteint** dans les **Options** ou sur la page **État** : brancher le câble ou le redémarrer.
4. Sans carte, ou avec une carte que l'appareil ne peut pas lire, l'appareil démarre quand même, avec les valeurs par défaut : pas de contacts, pas de boutons d'appel, donc pas de tuiles **Appels**, **Écrire** et **SOS**. Remettre la carte, ou lire le journal comme à l'étape suivante.
5. Si rien n'aide, brancher le câble USB à un ordinateur : la console de l'appareil montre le journal et les fichiers de la carte (`scripts/deviceconfig.py`, [device-console.md](../device-console.md), en anglais). Ouvrir le port redémarre l'appareil.
