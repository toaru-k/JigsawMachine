> **Note:** This document was translated by Generative AI. I recommend reading the original [Japanese version](README.md) for the most accurate information.

# JigsawMachine.exe

JigsawMachine.exe is a puzzle game that generates jigsaw puzzles from any .png, .jpeg, or .bmp image.
This game was created as an entry for the 1.44MB GAME_DEV CONTEST.

## How to Play

There are three buttons on the top right of the game screen. From left to right, they are: Upload, Save, and Load.

![sample1.png](sample1.png)

### Upload

Use the Upload button when you want to generate a new puzzle.
Click the upload button and select an image of your choice (.png, .jpeg, .bmp).
In the dialog that appears next, you can select the difficulty level.
The puzzle pieces will then be generated automatically and stored in the inventory on the right.

Drag and drop pieces from the inventory onto the playground to connect them all.
The more complex the image, the finer the pieces and the more complex the puzzle becomes.

You can also upload an image by dragging and dropping an image file directly from the File Explorer.
To get started, try playing with the included `sample1.png` and `sample2.jpg`.

### Save / Load

You can save your gameplay progress at any time.
Saved states can be restored using the Load button. It is perfectly fine to close the game in the meantime.

## Game Concept

When trying to fit a game into 1.44MB, the most difficult challenge is securing enough content. Images, in particular, cause data size to bloat.

To solve this, I focused on the fact that every computer already has images saved on it. A home-cooked meal you were proud of, a funny moment in a game, a beloved pet—people today save everything as images. If game content could be generated from them, the amount of content would be virtually infinite.

When thinking of game content generated from images, jigsaw puzzles are the first thing that comes to mind. They are simple yet fun.

However, the problem was *how* to generate the pieces from the image. Simply splitting it along a grid is boring, but decomposing pieces by visual elements usually requires complex processing.

Therefore, I intentionally reduced the color expression capability of the images. I restricted the number of bits allocated to each RGB channel to 4 bits.

This not only creates a retro atmosphere reminiscent of the floppy disk era, but it also merges similar colors to form reasonably sized pieces. Moreover, this segmentation naturally groups parts of the same visual elements, allowing the generation of complex puzzle pieces using a simple algorithm that easily fits within 1.44MB.

JigsawMachine.exe was built upon these ideas.
