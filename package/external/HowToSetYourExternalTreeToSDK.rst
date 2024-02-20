How to Set Your External Package Tree to SDK
============================================

Step1: Create your package tree base on "package/external/example_tree"
------------------------------------------------------------------------

1. Rename folder name "example_tree" to "your_tree" name
2. Rename the "name" value in "your_tree"/external.desc from "EXAMPLE_TREE" to "YOUR_TREE"
3. Rename the "BR2_EXTERNAL_EXAMPLE_TREE_PATH" to "BR2_EXTERNAL_YOUR_TREE_PATH" in
   "your_tree"/Config.in
   "your_tree"/external.mk

BTW: You can create "your_tree" in any place, e.g. beside on SDK folder.

Step2: Copy "package/external/external.mk" to SDKROOT
-----------------------------------------------------

1. Update LUBAN_EXTERNAL value in SDKROOT/external.mk, e.g.
   LUBAN_EXTERNAL=../your_tree

Step3: Add your package
-----------------------

Add your package setting to "your_tree"/package, and your code to "your_tree"/source


