<p align="center">
  <img src="docs/Vulkan_Logo.png" alt="VulkanGraphic, renderer Vulkan en C++" width="100%">
</p>

<p align="center">
  <strong>Mon projet Vulkan en C++</strong><br>
  Un petit renderer, une démo et un client de test.
</p>

<p align="center">
  <a href="#le-projet">Le projet</a> |
  <a href="#organisation">Organisation</a> |
  <a href="#compiler-et-lancer">Compiler et lancer</a> |
  <a href="#ou-j-en-suis">Où j'en suis</a>
</p>

---

## Le projet

La démo affiche un rectangle 2D en couleur dans une fenêtre GLFW. Le renderer
transfère ses sommets et ses indices vers le GPU avec des buffers intermédiaires.

J'ai séparé le renderer dans une bibliothèque `Renderer`. La démo `VulkanGraphic`
et un petit programme `ClientTest` utilisent cette même bibliothèque.

Pour être clair sur le périmètre : cette étape sert à mieux organiser le code et à
réutiliser le renderer. Je n'ai pas ajouté les transformations 3D, les textures ni
le depth buffer.

### À propos du refactoring

J'ai utilisé une intelligence artificielle pour m'aider à refactoriser et à organiser
le projet. Je ne lui ai pas demandé de générer tout le code d'un coup : j'ai suivi
les conseils et les étapes du tutoriel, et je les ai appliqués à mon projet.

## Organisation

| Fichier ou dossier | À quoi il sert |
| --- | --- |
| `Renderer/include/Renderer.h` | Interface utilisée par les applications |
| `Renderer/src/` | Initialisation Vulkan, swapchain, pipeline, buffers et rendu |
| `VulkanGraphic/` | La démo et son rectangle |
| `ClientTest/` | Un autre petit programme qui réutilise le renderer |
| `VulkanGraphic/shaders/` | Shaders GLSL utilisés par les deux programmes |

Chaque application compile ses shaders et garde ses fichiers de sortie dans son
propre dossier. On peut donc compiler le client de test sans commencer par la démo.

## Ce qu'il faut

- Windows et Visual Studio avec le toolset C++ `v145`
- Vulkan SDK `1.4.357.0`, avec `glslc.exe` et les validation layers Khronos
- GLFW `3.5` (en-têtes dans `include/GLFW`, bibliothèques dans `libs/GLFW`)

Les chemins du SDK Vulkan et de GLFW sont inscrits dans les fichiers de projet. Si
les bibliothèques sont ailleurs sur votre machine, il faudra ajuster ces chemins.

## Compiler et lancer

Ouvrez `VulkanGraphic.slnx` dans Visual Studio ou Rider et compilez en `Debug | x64`.
Ou, depuis un terminal développeur Visual Studio, lancez :

```powershell
msbuild VulkanGraphic.slnx /t:Build /p:Configuration=Debug /p:Platform=x64
```

Pour compiler en `Release` :

```powershell
msbuild VulkanGraphic.slnx /t:Build /p:Configuration=Release /p:Platform=x64
```

Pour compiler seulement le client de test :

```powershell
msbuild ClientTest/ClientTest.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64
```

Pour compiler seulement la démo :

```powershell
msbuild VulkanGraphic/VulkanGraphic.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64
```

Les shaders sont compilés automatiquement. Chaque application retrouve ses shaders à
côté de son exécutable ; ses fichiers de compilation restent dans son propre dossier
`build`.

Après une compilation `Debug | x64`, lancez l'un de ces exécutables :

- Démo : `VulkanGraphic/build/x64/Debug/VulkanGraphic-IndexBuffer.exe`
- Client de test : `ClientTest/build/x64/Debug/ClientTest.exe`

En `Release`, ils se trouvent dans les dossiers `build/x64/Release` correspondants.
La démo s'appelle alors `VulkanGraphic.exe`.

## Utiliser le renderer

Le client crée une fenêtre GLFW, passe la fenêtre et la géométrie au renderer, puis
appelle `DrawFrame` dans sa boucle d'événements. En cas de redimensionnement, il appelle
`NotifyFramebufferResized`. Avant de fermer la fenêtre, il appelle `Shutdown`.

Les tableaux donnés à `Initialize` doivent rester valides pendant l'appel. Le renderer
en copie les données vers le GPU avant de retourner.

## Où j'en suis

J'ai vérifié les points suivants :

| Vérification | Résultat |
| --- | --- |
| Compilation complète en `Debug` et `Release` | Réussie |
| Compilation séparée des deux applications | Réussie |
| Ouverture des fenêtres et affichage du rectangle | Vérifiés |
| Redimensionnement, minimisation puis restauration | Vérifiés |
| Messages des validation layers pendant les lancements | Aucun reçu |

Le redimensionnement a révélé un cas où Vulkan-Hpp interrompait la récupération de
la swapchain. J'ai ajusté sa configuration pour que le renderer puisse gérer ce cas.
Je n'ai toutefois pas fait de vérification exhaustive des validation layers.

Il reste un avertissement `LNK4098` lors de la compilation Debug, lié au runtime
déclaré par la bibliothèque GLFW statique. Je ne l'ai pas masqué. Les chemins des
dépendances sont également propres à mon installation Windows. Les textures, les
transformations 3D et le depth buffer restent à faire plus tard.
