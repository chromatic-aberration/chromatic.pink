import crafttweaker.api.recipe.type.Recipe;

<recipetype:apotheosis:salvaging>.addJsonRecipe("block_fluorite_random", {
    type: "apotheosis:salvaging",
    conditions: [
        {
            type: "apotheosis:module",
            module: "adventure"
        }
    ],
    input: {
        item: "mekanism:block_fluorite"
    },
    outputs: [
        {
            min_count: 0,
            max_count: 1,
            stack: {
                item: "apotheosis:gem_dust"
            }
        },
        {
            min_count: 0,
            max_count: 1,
            stack: {
                item: "ae2:certus_quartz_crystal"
            }
        },
        {
            min_count: 0,
            max_count: 2,
            stack: {
                item: "mekanism:crystal_osmium"
            }
        },
        {
            min_count: 0,
            max_count: 2,
            stack: {
                item: "mekanism:crystal_iron"
            }
        },
        {
            min_count: 0,
            max_count: 2,
            stack: {
                item: "mekanism:crystal_uranium"
            }
        },
        {
            min_count: 0,
            max_count: 2,
            stack: {
                item: "minecraft:glowstone_dust"
            }
        }
    ]
});
