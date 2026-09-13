using UnrealBuildTool;

public class AshenHollow : ModuleRules
{
    public AshenHollow(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
            "GameplayAbilities", "GameplayTags", "GameplayTasks", "UMG"
        });
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Json", "JsonUtilities", "NavigationSystem", "AIModule", "Slate", "SlateCore", "RenderCore", "AnimGraphRuntime"
        });
    }
}
