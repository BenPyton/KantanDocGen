# Kantansaurus Plugin

This is a fork of the great KantanDocGen plugin with some changes, from quality of life changes to new data added into the generated docs. 

Generate documentation from your C++ code (tooltips, comments and various UE macros) into markdown files usable directly in a [Docusaurus](https://docusaurus.io/) website.

## How to Use

### Writing Documentation

You write your documentation directly in your code.

In C++, you can use some meta specifiers in the UE macros, like the `DisplayName` of the `Tooltip` to set respectively the displayed name and the description in the documentation pages.

For example:
```cpp
UCLASS(Blueprintable, meta=(DisplayName="My Great Actor" Tooltip="This is my great actor!"))
class AMyActor : public AActor
{
	...
}
```
Alternatively to the tooltip, you can add comments above the UE macros to generate the descriptions:
```cpp
// This is my great actor!
UCLASS(Blueprintable, meta=(DisplayName="My Great Actor"))
class AMyActor : public AActor
{
	...
}
```
For the `FUNCTION()` macros, you can use the Doxygen syntax to set description for the return type and parameters:
```cpp
// This is the description of my great function!
// @param InputA This is the description of InputA
// @param InputB This is the description of InputB
// @return This is the description of the return value
UFUNCTION(BlueprintCallable, meta=(DisplayName="My Great Function!"))
bool MyFunction(FString InputA, int InputB)
{
	...
}
```

In blueprint, you should be able to set the display name and description from the class settings in editor.

### Generating Documentation

Got to `Tools` -> `Generate Documentation` to open up a dialog letting you to select which modules to document.\

The output directory can be anywhere.\
Typically in a subfolder of your Docusaurus `/docs/` directory.

Click the `Generate` button and wait until the plugin has finished.

Your documentation is ready to be placed in a Docusaurus website.

You can directly build and deploy your Docusaurus after that.

This plugin uses [TransmuDoc](https://github.com/BenPyton/TransmuDoc) as a third party xml transformation tool instead of the original [KantanDocGenTool](https://github.com/kamrann/KantanDocGenTool).\
It is included inside this plugin so does not need to be installed separately.
