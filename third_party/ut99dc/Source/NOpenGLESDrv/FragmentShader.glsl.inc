R""(

varying mediump vec3 vPosition;
#ifdef SF_Texture0
varying mediump vec2 vTexCoord0;
#endif
#ifdef SF_Texture1
varying mediump vec2 vTexCoord1;
#endif
#ifdef SF_Texture2
varying mediump vec2 vTexCoord2;
#endif
#ifdef SF_Texture3
varying mediump vec2 vTexCoord3;
#endif
#ifdef SF_VtxColor
varying lowp vec4 vVtxColor;
#endif
#ifdef SF_VtxFog
varying lowp vec4 vVtxFog;
#endif

uniform lowp float uBrightness;

#ifdef SF_Texture0
uniform sampler2D uTexture0;
#endif
#ifdef SF_Texture1
uniform sampler2D uTexture1;
#endif
#ifdef SF_Texture2
uniform sampler2D uTexture2;
#endif
#ifdef SF_Texture3
uniform sampler2D uTexture3;
#endif

#ifndef LIGHTMAP_OVERBRIGHT
#define LIGHTMAP_OVERBRIGHT 1.0
#endif

void main()
{
	lowp vec4 Out = vec4(1.0);

#ifdef SF_Texture0
	lowp vec4 Texel0 = texture2D(uTexture0, vTexCoord0);
	Out = Texel0;
#endif

#ifdef SF_AlphaTest
	if( Out.a < 0.5 )
		discard;
#endif

#ifdef SF_ModulatedDecalMask
	// UT99_ANDROID_V166G_MODULATED_DECAL_MASK:
	// The original shadow/decal stays modulated.  Only very light grey/white
	// background pixels are discarded so the blob shadow remains but the square
	// border around it disappears.
	lowp float ModulatedDecalLuma = max( max( Out.r, Out.g ), Out.b );
	if( ModulatedDecalLuma > 0.42 )
		discard;
#endif

#ifdef SF_Texture1
	lowp vec4 Texel1 = texture2D(uTexture1, vTexCoord1);
#ifdef SF_Lightmap
	Out.rgb *= Texel1.rgb * 2.0 * LIGHTMAP_OVERBRIGHT;
#else
	Out *= Texel1;
#endif
#endif

#ifdef SF_Texture2
	lowp vec4 Texel2 = texture2D(uTexture2, vTexCoord2);
#ifdef SF_Fogmap
	Texel2 *= 2.0;
	Out.rgb = Texel2.rgb + Out.rgb * (1.0 - Texel2.a);
	Out.a = Texel2.a;
#else
	Out *= Texel2;
#endif
#endif

#ifdef SF_Texture3
	lowp vec4 Texel3 = texture2D(uTexture3, vTexCoord3) * 2.0;
#ifdef SF_Detail
	// TODO: XOpenGLDrv does some RGB->HSV->RGB magic on detail maps, we just * 2 instead
	lowp float DetailScale = clamp(0.65 - vPosition.z / 380.0, 0.0, 1.0);
	Texel3 = mix(vec4(1.0), Texel3, DetailScale);
	Out.rgb *= Texel3.rgb;
#else
	Out.rgb *= Texel3.rgb;
#endif
#endif

#ifdef SF_VtxColor
	Out *= vVtxColor;
#endif

#ifdef SF_VtxFog
	Out.rgb = vVtxFog.rgb + Out.rgb * (1.0 - vVtxFog.a);
#endif

	Out.rgb *= 0.5 + uBrightness;

	gl_FragColor = Out;
}

)""
