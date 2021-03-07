#pragma once

// TODO Refactor common code from PbrGraphicsPipelineStage and ShadowMapGraphicsPipelineStage here.

// GOAL: new class that efficiently draws huge amounts of render primitives

// This only does the drawing! 3d scene object culling, etc is done externally

// Descriptor set levels common for all objects
// 1. Pass
// 2. Material
// 3. Mesh Draw
// 4. Constant buffers?