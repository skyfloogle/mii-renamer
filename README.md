# Mii Renamer

Mii Renamer is a Wii homebrew app that allows for renaming Miis, including those created on another Wii.

## Why?
Miis are stored in a list in the Wii's memory, with space for 100 Miis.
Every Mii stores a unique ID for the console it was created on. If the Mii Channel detects attempts to
modify a Mii created on another Wii, it refuses to edit the Mii.
Editing the unique ID, or moving a Mii to a different position in the list, breaks
save data for a number of early Wii series games.

[SaveGame Manager GX](https://wiibrew.org/wiki/SaveGame_Manager_GX) allows for importing and exporting Miis.
If an imported Mii has the same name as one already present on the system, it overwrites that Mii.
This allows for modifying Miis without breaking save data, with the exception of the Mii's name.

Enter Mii Renamer: a tool that lets you rename Miis directly on your Wii, without breaking your save data.

## How?
Using Mii Renamer is simple: select the Mii you want to edit with the Wii Remote's D-Pad and A button,
then rename the Mii using the D-Pad. Names can be up to 10 characters long.
If your Mii's name doesn't fit entirely within ASCII, its name cannot currently be edited.
When you're done, press + on the main menu to save your changes.
