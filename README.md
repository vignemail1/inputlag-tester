# inputlag-tester

Petit utilitaire Windows pour mesurer la latence **input → capture DXGI** d'un jeu/application en plein écran ou fenêtré.

> ⚠️ Important : ce programme mesure le temps entre un mouvement souris et la détection du changement d'image par DXGI.  
> Il ne mesure pas directement le temps d'affichage réel (scanout + réponse du panneau).

## Fonctionnalités

- Détection automatique du **taux de rafraîchissement** (Hz) via DXGI.
- Mesure de la latence en **millisecondes** et en **nombre de frames**.
- Statistiques : min, médiane, moyenne, p95, p99, max, écart-type.
- Configurable (nombre d'échantillons, intervalle, zone de capture, etc.).

## Compilation locale

Pré-requis : Visual Studio avec toolset C++ (MSVC) ou Build Tools.

```powershell
cl /std:c++17 inputlag-tester.cpp /link dxgi.lib d3d11.lib kernel32.lib user32.lib
```

L'exécutable `inputlag-tester.exe` sera généré dans le répertoire courant.

## Utilisation

1. Vous lancez votre jeu
2. Vous lancez un invite de commande ou Powershell
3. drag-n-drop du fichier .exe dans la fenetre invite de commande puis vous complètez avec vos options si besoin
4. vous revenez vite dans le jeu (en moins de 3 secondes)

```powershell
inputlag-tester.exe -n 100 -interval 200 -warmup 10
```

Options :

- `-n` : nombre total de mesures (défaut: 210)
- `-warmup` : échantillons ignorés au début (défaut: 10)
- `-interval` : temps entre deux mouvements souris en ms (défaut: 50)
- `-x <X> -y <Y>` : coin haut gauche de la zone de capture (0,0 = coin haut gauche de l'écran)
- `-w <largeur> -h <hauteur>` : taille de la zone de capture (par défaut : carré 200x200 centré)
- `-dx` : amplitude du mouvement souris horizontal (défaut: 30)

## Interprétation des résultats

- Ce qui est mesuré :  
  `mouvement souris -> frame modifiée vue par DXGI`
- Inclus : moteur du jeu, GPU, composition Windows, capture DXGI.
- Non inclus : temps de scanout de l'écran, temps de réponse du panneau.

Pour une mesure **input-to-photon** physique (vraie latence jusqu'à la lumière émise par l'écran), il faut une caméra haute vitesse ou une photodiode placée sur l'écran.

## Releases

Des builds Windows prêts à l'emploi sont publiés automatiquement dans l'onglet **Releases** à chaque tag `vX.Y.Z`.
