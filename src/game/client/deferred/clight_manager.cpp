
#include "cbase.h"
#include "deferred/deferred_shared_common.h"
#include "CollisionUtils.h"

#include "viewrender.h"
#include "view_shared.h"
#include "engine/IVDebugOverlay.h"
#include "tier0/fasttimer.h"
#include "tier1/callqueue.h"

static CLightingManager __g_lightingMan;
CLightingManager *GetLightingManager()
{
	return &__g_lightingMan;
}

static void LightingResourceRelease( int nChangeFlags )
{
	GetLightingManager()->OnMaterialReload();
}

CLightingManager::CLightingManager() : BaseClass( "LightingManagerSystem" )
{
#if DEBUG
	m_bVolatileLists = false;
#endif

	m_bDrawWorldLights = true;

	MatrixSetIdentity( m_matScreenToWorld );

	m_vecViewOrigin.Init();
	m_vecForward.Init();
	m_flzNear = 0;
	m_bDrawVolumetrics = false;
}

CLightingManager::~CLightingManager()
{
}

bool CLightingManager::Init()
{
	materials->AddReleaseFunc( LightingResourceRelease );
	return true;
}

void CLightingManager::Shutdown()
{
	Assert( m_hDeferredLights.Count() == 0 );
	materials->RemoveReleaseFunc( LightingResourceRelease );
}

void CLightingManager::LevelInitPostEntity()
{
}

void CLightingManager::LevelShutdownPostEntity()
{
	m_hRenderLights.Purge();

	for ( int i = 0; i < LSORT_COUNT; i++ )
		m_hPreSortedLights[ i ].Purge();
}

void CLightingManager::Update( float ft )
{
}

void CLightingManager::SetRenderWorldLights( bool bRender )
{
	m_bDrawWorldLights = bRender;
}

void CLightingManager::SetRenderConstants( const VMatrix &ScreenToWorld,
		const CViewSetup &setup )
{
	m_matScreenToWorld = ScreenToWorld;

	m_vecViewOrigin = setup.origin;
	AngleVectors( setup.angles, &m_vecForward );

	m_flzNear = setup.zNear;
}

void CLightingManager::LightSetup( const CViewSetup &setup )
{
	PrepareLights();

#if DEBUG
	m_bVolatileLists = true;
#endif

	CullLights();

	SortLights();

	if ( deferred_lightmanager_debug.GetInt() >= 2 )
	{
		DebugLights_Draw_Boundingboxes();
	}
}

void CLightingManager::LightTearDown()
{
	DoSceneDebug();

	ClearTmpLists();

#if DEBUG
	m_bVolatileLists = false;
#endif
}

void CLightingManager::OnCookieStringReceived( const char *pszString, const int &index )
{
	ITexture *pTexCookie = materials->FindTexture( pszString, TEXTURE_GROUP_OTHER );

	if ( IsErrorTexture( pTexCookie ) )
		return;
}

void CLightingManager::OnMaterialReload()
{
	FOR_EACH_VEC_FAST( def_light_t*, m_hDeferredLights, l )
	{
		l->ClearCookie();

		l->MakeDirtyAll();
	}
	FOR_EACH_VEC_FAST_END
}

void CLightingManager::AddLight( def_light_t *l )
{
	Assert( !m_hDeferredLights.HasElement( l ) );

	m_hDeferredLights.AddToTail( l );
}

bool CLightingManager::RemoveLight( def_light_t *l )
{
#if DEBUG
	AssertMsg( m_bVolatileLists == false, "You MUST NOT remove lights while rendering." );
#endif

	return m_hDeferredLights.FindAndRemove( l );
}

bool CLightingManager::IsLightRendered( def_light_t *l )
{
#if DEBUG
	AssertMsg( m_bVolatileLists, "Only works correctly during rendering." );
#endif

	return m_hRenderLights.HasElement( l );
}

void CLightingManager::ClearTmpLists()
{
	m_hRenderLights.RemoveAll();

	for ( int i = 0; i < LSORT_COUNT; i++ )
		m_hPreSortedLights[ i ].RemoveAll();
}

void CLightingManager::PrepareLights()
{
	FOR_EACH_VEC_FAST( def_light_t*, m_hDeferredLights, l )
	{
		l->UpdateCookieTexture();

		if ( !l->IsDirty() )
		{
			continue;
		}

		if ( l->IsDirtyXForms() )
		{
			if ( l->IsSpot() )
				l->UpdateMatrix();

			l->UpdateXForms();
		}

		if ( l->IsDirtyRenderMesh() )
		{
			l->UpdateRenderMesh();
			l->UpdateVolumetrics();
		}

		l->UnDirtyAll();
	}
	FOR_EACH_VEC_FAST_END
}

void CLightingManager::CullLights()
{
	Assert( m_hRenderLights.Count() == 0 );

	Vector veclightDelta;

	FOR_EACH_VEC_FAST( def_light_t*, m_hDeferredLights, l )
	{
		if ( !m_bDrawWorldLights && l->bWorldLight )
			continue;

		if ( !render->AreAnyLeavesVisible( l->iLeaveIDs, l->iNumLeaves ) )
			continue;

		// if the optimized bounds cause popping for you, use the naive ones or
		// ...improve the optimization code

		if ( engine->CullBox( l->bounds_min_naive, l->bounds_max_naive ) )
		//if ( engine->CullBox( l->bounds_min, l->bounds_max ) )
			continue;

		if ( l->IsSpot() && l->HasShadow() )
		{
			if ( IntersectFrustumWithFrustum( m_matScreenToWorld, l->spotMVPInv ) )
				continue;
		}

		veclightDelta = l->boundsCenter - m_vecViewOrigin;

		if ( veclightDelta.LengthSqr() > l->flMaxDistSqr )
			continue;

		l->flDistance_ViewOrigin = veclightDelta.Length();
		l->flShadowFade = l->HasShadow() ?
			( SATURATE( ( l->flDistance_ViewOrigin - l->iShadow_Dist ) / l->iShadow_Range ) )
			: 1.0f;

		m_hRenderLights.AddToTail( l );
	}
	FOR_EACH_VEC_FAST_END
}

void CLightingManager::SortLights()
{
#if DEBUG
	for ( int i = 0; i < LSORT_COUNT; i++ )
		Assert( m_hPreSortedLights[ i ].Count() == 0 );
#endif

	m_bDrawVolumetrics = false;

	FOR_EACH_VEC_FAST( def_light_t*, m_hRenderLights, l )
	{
		float zNear = m_flzNear + 2;
		Vector vecBloat( zNear, zNear, zNear );

		bool bNeedsFullscreen = IsPointInBounds( m_vecViewOrigin,
			l->bounds_min_naive - vecBloat,
			l->bounds_max_naive + vecBloat );

		if ( bNeedsFullscreen && l->IsSpot() )
		{
			Vector camMins( m_vecViewOrigin - vecBloat );
			Vector camMaxs( m_vecViewOrigin + vecBloat );
			bNeedsFullscreen = !l->spotFrustum.CullBox( camMins, camMaxs );
		}

		const bool bVolume = ( l->HasShadow() && l->HasVolumetrics() );

		m_bDrawVolumetrics = m_bDrawVolumetrics || bVolume;

		Assert( l->iLighttype * 2 + 1 < LSORT_COUNT );
		m_hPreSortedLights[ l->iLighttype * 2 + (int)bNeedsFullscreen ].AddToTail( l );
	}
	FOR_EACH_VEC_FAST_END

	static CUtlVector< def_light_t* > hBatchFullscreen;

	for ( int i = 0; i < 2; i++ )
	{
		FOR_EACH_VEC_FAST( def_light_t*, m_hPreSortedLights[ i * 2 ], l )
		{
			if ( l->HasShadow() ||
				l->HasCookie() ||
				l->HasVolumetrics() )
				continue;

			const bool bSpot = l->IsSpot();
			const float flSizeFactor = bSpot ? 1.5f : 2.25f;

			Vector viewVec = l->boundsCenter - m_vecViewOrigin;
			const float flDot = DotProduct( m_vecForward, viewVec.Normalized() );
			const float flCoverageFactor = flSizeFactor * l->flRadius / l->flDistance_ViewOrigin * flDot;

			if ( flCoverageFactor > 1 )
				hBatchFullscreen.AddToTail( l );
		}
		FOR_EACH_VEC_FAST_END

		if ( hBatchFullscreen.Count() > 1 )
		{
			FOR_EACH_VEC_FAST( def_light_t*, hBatchFullscreen, l )
			{
#if DEBUG
				Assert( m_hPreSortedLights[ i * 2 ].FindAndRemove( l ) );
#else
				m_hPreSortedLights[ i * 2 ].FindAndRemove( l );
#endif
				m_hPreSortedLights[ i * 2 + 1 ].AddToTail( l );
			}
			FOR_EACH_VEC_FAST_END
		}

		hBatchFullscreen.RemoveAll();
	}
}

FORCEINLINE float CLightingManager::DoLightStyle( def_light_t *l )
{
	if ( !l->HasLightstyle() )
		return 1.0f;

	if ( gpGlobals->curtime > l->flLastRandomTime )
	{
		l->flLastRandomTime = gpGlobals->curtime + 1.0f - MIN( 1, l->flStyle_Speed );

		random->SetSeed( l->iStyleSeed );
		l->flLastRandomValue = 1.0f - random->RandomFloat( 0, MIN( 1, l->flStyle_Amount ) );
	}

	float ran = l->flLastRandomValue;

	float sine = FastCos( gpGlobals->curtime * l->flStyle_Speed + l->iStyleSeed ) * 0.5f + 0.5f;
	sine = Bias( sine, l->flStyle_Smooth );

	return Lerp( l->flStyle_Random, Lerp( l->flStyle_Amount, 1.0f, sine ), ran );
}

/* Which data to commit and how:

	Point lights:

	position		x y z	radius			w
	color diffuse	x y z	falloffpower	w
	color ambient	x y z

	** shadowed or cookied **
							shadowscale		w

	rotationmatrix	x y z
					x y z
					x y z

	max num constants: 10 * 3 +				-- simple
						3 * 6 +				-- cookie
						3 * 6 +				-- shadowed
						2 * 6				-- shadowed cookie


	Spot lights:

	position			x y z	radius			w
	color diffuse		x y z	falloffpower	w
	color ambient		x y z	cone outer		w
	backward			x y z	cone inner		w

	** shadowed or cookied **
	shadowscale			x

	world2texture		x y z w
						x y z w
						x y z w
						x y z w

	max num constants: 10 * 4 +				-- simple
						3 * 9 +				-- cookie
						3 * 9 +				-- shadowed
						2 * 9				-- shadowed cookie
	*/

FORCEINLINE int CLightingManager::WriteLight( def_light_t *l, float *pfl4 )
{
	const bool bShadow = l->ShouldRenderShadow();
	const bool bCookie = l->HasCookie();

	const bool bAdvanced = bShadow || bCookie;

	int numConsts = 0;

	float flLightstyle = DoLightStyle( l );
	float flDistance = l->flDistance_ViewOrigin;
	float flMasterFade = flLightstyle * ( 1.0f - SATURATE( ( flDistance - l->iVisible_Dist ) / l->iVisible_Range ) );

	const int iFloatSize = sizeof( float );

	Vector colDiffuse = l->col_diffuse * flMasterFade;
	Vector colAmbient = l->col_ambient * flMasterFade;

	switch ( l->iLighttype )
	{
	default:
		Assert(0);
	case DEFLIGHTTYPE_POINT:
		{
			numConsts = bAdvanced ? NUM_CONSTS_POINT_ADVANCED : NUM_CONSTS_POINT_SIMPLE;

			Q_memcpy( pfl4, l->pos.Base(), iFloatSize * 3 );
			pfl4 += 3;

			*pfl4 = l->flRadius;
			pfl4++;

			Q_memcpy( pfl4, colDiffuse.Base(), iFloatSize * 3 );
			pfl4 += 3;

			*pfl4 = l->flFalloffPower;
			pfl4++;

			Q_memcpy( pfl4, colAmbient.Base(), iFloatSize * 3 );
			pfl4 += 3;

			if ( bAdvanced )
			{
				*pfl4 = l->flShadowFade;
				pfl4++;

				VMatrix rotMatrix, rotMatrixit;
				rotMatrix.Identity();
				rotMatrix.SetupMatrixOrgAngles( vec3_origin, l->ang );
				MatrixSourceToDeviceSpaceInv( rotMatrix );
				rotMatrix = rotMatrix.Transpose3x3();
				MatrixInverseGeneral( rotMatrix, rotMatrixit );
				Q_memcpy( pfl4, rotMatrixit.Base(), iFloatSize * 12 );
			}
		}
		break;
	case DEFLIGHTTYPE_SPOT:
		{
			numConsts = bAdvanced ? NUM_CONSTS_SPOT_ADVANCED : NUM_CONSTS_SPOT_SIMPLE;

			Q_memcpy( pfl4, l->pos.Base(), iFloatSize * 3 );
			pfl4 += 3;

			*pfl4 = l->flRadius;
			pfl4++;

			Q_memcpy( pfl4, colDiffuse.Base(), iFloatSize * 3 );
			pfl4 += 3;

			*pfl4 = l->flFalloffPower;
			pfl4++;

			Q_memcpy( pfl4, colAmbient.Base(), iFloatSize * 3 );
			pfl4 += 3;

			*pfl4 = l->flSpotCone_Outer;
			pfl4++;

			Q_memcpy( pfl4, l->backDir.Base(), iFloatSize * 3 );
			pfl4 += 3;

			*pfl4 = l->flSpotCone_Inner;
			pfl4++;

			if ( bAdvanced )
			{
				*pfl4 = l->flShadowFade;
				pfl4 += 4;

				Q_memcpy( pfl4, l->spotWorldToTex.Base(), iFloatSize * 16 );
			}
		}
		break;
	}

	return numConsts;
}

FORCEINLINE bool SortLightsByComboType( CUtlVector<def_light_t*> &hLights,
	CUtlVector<def_light_t*> &hLightsShadowedCookie, CUtlVector<def_light_t*> &hLightsShadowed,
	CUtlVector<def_light_t*> &hLightsCookied, CUtlVector<def_light_t*> &hLightsSimple )
{
	hLightsShadowedCookie.RemoveAll();
	hLightsShadowed.RemoveAll();
	hLightsCookied.RemoveAll();
	hLightsSimple.RemoveAll();

	FOR_EACH_VEC_FAST( def_light_t*, hLights, l )
	{
		const bool bShadowed = l->ShouldRenderShadow();
		const bool bCookie = l->HasCookie() && l->IsCookieReady();

		if ( bShadowed && bCookie )
			hLightsShadowedCookie.AddToTail( l );
		else if ( bShadowed )
			hLightsShadowed.AddToTail( l );
		else if ( bCookie )
			hLightsCookied.AddToTail( l );
		else
			hLightsSimple.AddToTail( l );
	}
	FOR_EACH_VEC_FAST_END

	Assert( hLights.Count() ==
		hLightsShadowedCookie.Count() + hLightsShadowed.Count() +
		hLightsCookied.Count() + hLightsSimple.Count() );

	return hLights.Count() > 0;
}

FORCEINLINE void CLightingManager::DrawVolumePrepass( bool bDoModelTransform, const CViewSetup &view, def_light_t *l )
{
	if ( !l->IsSpot() )
		return;

	Assert( l->pMesh_VolumPrepass != NULL );

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PushRenderTargetAndViewport( GetDefRT_VolumePrepass(), 0, 0, view.width / 4, view.height / 4 );
	pRenderContext->ClearColor3ub( 0, 0, 0 );
	pRenderContext->ClearBuffers( true, false );

	if ( bDoModelTransform )
	{
		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PushMatrix();
		pRenderContext->LoadMatrix( l->worldTransform );
	}

	pRenderContext->Bind( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_VOLUME_PREPASS ) );
	l->pMesh_VolumPrepass->Draw();

	if ( bDoModelTransform )
	{
		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PopMatrix();
	}

	pRenderContext->PopRenderTargetAndViewport();
}

void CLightingManager::RenderLights( const CViewSetup &view, CDeferredViewRender *pCaller )
{
	static CUtlVector<def_light_t*> lightsShadowedCookied;
	static CUtlVector<def_light_t*> lightsShadowed;
	static CUtlVector<def_light_t*> lightsCookied;
	static CUtlVector<def_light_t*> lightsSimple;

	struct Lightpass_t
	{
		CUtlVector<def_light_t*> *lightVecFullscreen;
		CUtlVector<def_light_t*> *lightVecWorld;
		IMaterial *pMatPassFullscreen;
		IMaterial *pMatPassWorld;
		IMaterial *pMatVolumeFullscreen;
		IMaterial *pMatVolumeWorld;
		int constCount_simple;
		int constCount_advanced;
	};

	Lightpass_t lightTypes[] =
	{
		{ &m_hPreSortedLights[ LSORT_POINT_FULLSCREEN ], &m_hPreSortedLights[ LSORT_POINT_WORLD ],
		GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_POINT_FULLSCREEN ),
		GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_POINT_WORLD ),
		GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_VOLUME_POINT_FULLSCREEN ),
		GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_VOLUME_POINT_WORLD ),
		NUM_CONSTS_POINT_SIMPLE, NUM_CONSTS_POINT_ADVANCED },

		{ &m_hPreSortedLights[ LSORT_SPOT_FULLSCREEN ], &m_hPreSortedLights[ LSORT_SPOT_WORLD ],
		GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_SPOT_FULLSCREEN ),
		GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_SPOT_WORLD ),
		GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ),
		GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ),
		NUM_CONSTS_SPOT_SIMPLE, NUM_CONSTS_SPOT_ADVANCED },
	};
	const int iNumLightTypes = ARRAYSIZE( lightTypes );

	int iPassesDrawnFullscreen[iNumLightTypes] = { 0 };
	int iPassesVolumetrics[2] = { 0 };
	int iDrawnShadowed = 0;
	int iDrawnCookied = 0;
	int iDrawnSimple = 0;

	ITexture *pVolumBuffer0 = GetDefRT_VolumetricsBuffer( 0 );

	if ( m_bDrawVolumetrics )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->PushRenderTargetAndViewport( pVolumBuffer0 );
		pRenderContext->ClearColor3ub( 0, 0, 0 );
		pRenderContext->ClearBuffers( true, false );
		pRenderContext->PopRenderTargetAndViewport();
	}

	struct defData_commitData
	{
	public:
		float *pData;
		int a,b,c,d,rows;

		static void Fire( defData_commitData d )
		{
			float *pflTrash = GetDeferredExt()->CommitLightData_Common(
				d.pData,
				d.rows,
				d.a, d.b,
				d.c, d.d );
			delete [] pflTrash;
		};
	};

	struct defData_Cookie
	{
	public:
		ITexture *pCookie;
		int index;
		static void Fire( defData_Cookie d )
		{
			GetDeferredExt()->CommitTexture_Cookie( d.index, d.pCookie );
		}
	};

	struct defData_Volume
	{
	public:
		volumeData_t mData;
		static void Fire( defData_Volume d )
		{
			GetDeferredExt()->CommitVolumeData( d.mData );
		}
	};

	struct queueVolume
	{
		queueVolume( def_light_t *l, int offset_data, int offset_sampler )
		{
			dataoffset = offset_data;
			sampleroffset = offset_sampler;
			pLight = l;
		};

		int dataoffset;
		int sampleroffset;
		def_light_t *pLight;
	};

	for ( int i = 0; i < iNumLightTypes; i++ )
	{
		if ( SortLightsByComboType( *(lightTypes[i].lightVecFullscreen),
			lightsShadowedCookied, lightsShadowed, lightsCookied, lightsSimple ) )
		{
			while ( lightsShadowedCookied.Count() > 0 || lightsShadowed.Count() > 0 ||
				lightsCookied.Count() > 0 || lightsSimple.Count() > 0 )
			{
				int drawShadowedCookied = MIN( MAX_LIGHTS_SHADOWEDCOOKIE, lightsShadowedCookied.Count() );
				int drawShadowed = MIN( MAX_LIGHTS_SHADOWED, lightsShadowed.Count() );
				int drawCookied = MIN( MAX_LIGHTS_COOKIE, lightsCookied.Count() );
				int drawSimple = MIN( MAX_LIGHTS_SIMPLE, lightsSimple.Count() );

				int numRows = ( drawShadowedCookied * lightTypes[i].constCount_advanced +
					drawShadowed * lightTypes[i].constCount_advanced +
					drawCookied * lightTypes[i].constCount_advanced +
					drawSimple * lightTypes[i].constCount_simple );

				int memToAlloc = 4 * numRows;

				Assert( memToAlloc > 0 );

				float *pFlLightDataBlock = new float[ memToAlloc ];

				Assert( pFlLightDataBlock != NULL );

				if ( pFlLightDataBlock != NULL )
				{
					static CUtlVector< def_light_t* > commitableLights;
					static CUtlVector< queueVolume > volumeLights;

					commitableLights.AddMultipleToTail( drawShadowedCookied, lightsShadowedCookied.Base() );
					commitableLights.AddMultipleToTail( drawShadowed, lightsShadowed.Base() );
					commitableLights.AddMultipleToTail( drawCookied, lightsCookied.Base() );
					commitableLights.AddMultipleToTail( drawSimple, lightsSimple.Base() );

					float *pFlWriter = pFlLightDataBlock;

					FOR_EACH_VEC_FAST( def_light_t*, commitableLights, l )
					{
						pFlWriter += WriteLight( l, pFlWriter ) * 4;
					}
					FOR_EACH_VEC_FAST_END

					Assert( pFlWriter - pFlLightDataBlock == memToAlloc );

					iPassesDrawnFullscreen[i]++;

					for ( int iShadowed = 0; iShadowed < (drawShadowedCookied+drawShadowed); iShadowed++ )
					{
						pCaller->DrawLightShadowView( view, iShadowed, commitableLights[ iShadowed ] );

						if ( commitableLights[ iShadowed ]->HasVolumetrics() )
						{
							int iDataOffset = iShadowed * lightTypes[i].constCount_advanced * 4;
							int iSamplerOffset = iShadowed;

							volumeLights.AddToTail( queueVolume( commitableLights[ iShadowed ], iDataOffset, iSamplerOffset ) );
						}
					}

					commitableLights.RemoveAll();

					defData_commitData data;
					data.pData = pFlLightDataBlock;
					data.rows = numRows;
					data.a = drawShadowedCookied;
					data.b = drawShadowed;
					data.c = drawCookied;
					data.d = drawSimple;
					QUEUE_FIRE( defData_commitData, Fire, data );

					iDrawnShadowed += drawShadowedCookied + drawShadowed;
					iDrawnCookied += drawShadowedCookied + drawCookied;
					iDrawnSimple += drawSimple;

					static CUtlVector< def_light_t* > cookiedLights;
					cookiedLights.AddVectorToTail( lightsShadowedCookied );
					cookiedLights.AddVectorToTail( lightsCookied );

					for ( int iCookied = 0; iCookied < (drawShadowedCookied+drawCookied); iCookied++ )
					{
						defData_Cookie data;
						data.index = iCookied;
						data.pCookie = cookiedLights[iCookied]->GetCookieForDraw( iCookied );

						Assert( data.pCookie != NULL );

						QUEUE_FIRE( defData_Cookie, Fire, data );
					}

					cookiedLights.RemoveAll();

					DrawLightPassFullscreen( lightTypes[i].pMatPassFullscreen, view.width, view.height );

					FOR_EACH_VEC_FAST( queueVolume, volumeLights, entry )
					{
						defData_Volume data;
						data.mData.iDataOffset = entry.dataoffset;
						data.mData.iSamplerOffset = entry.sampleroffset;
						data.mData.iNumRows = lightTypes[i].constCount_advanced;
						data.mData.bHasCookie = entry.pLight->HasCookie();
						QUEUE_FIRE( defData_Volume, Fire, data );

						DrawVolumePrepass( true, view, entry.pLight );

						CMatRenderContextPtr pRenderContext( materials );
						pRenderContext->PushRenderTargetAndViewport( pVolumBuffer0 );

						DrawLightPassFullscreen( lightTypes[i].pMatVolumeFullscreen,
							view.width / 4.0f, view.height / 4.0f );

						pRenderContext->PopRenderTargetAndViewport();

						iPassesVolumetrics[1]++;
					}
					FOR_EACH_VEC_FAST_END

					volumeLights.RemoveAll();
				}

				lightsShadowedCookied.RemoveMultipleFromHead( drawShadowedCookied );
				lightsShadowed.RemoveMultipleFromHead( drawShadowed );
				lightsCookied.RemoveMultipleFromHead( drawCookied );
				lightsSimple.RemoveMultipleFromHead( drawSimple );
			}
		}
	}

	for ( int i = 0; i < iNumLightTypes; i++ )
	{
		FOR_EACH_VEC_FAST( def_light_t*, (*(lightTypes[i].lightVecWorld)), l )
		{
			if ( l->pMesh_World == NULL )
				continue;

			const bool bShadow = l->ShouldRenderShadow();
			const bool bCookie = l->HasCookie() && l->IsCookieReady();
			const bool bVolumetrics = l->HasVolumetrics() && bShadow;

			const bool bAdvanced = ( bShadow || bCookie );

			int numRows = bAdvanced ? lightTypes[i].constCount_advanced : lightTypes[i].constCount_simple;
			int memToAlloc = 4 * numRows;

			Assert( memToAlloc > 0 );

			float *pFlLightDataBlock = new float[ memToAlloc ];

			WriteLight( l, pFlLightDataBlock );

			if ( bShadow )
			{
				pCaller->DrawLightShadowView( view, 0, l );

				iDrawnShadowed++;
			}

			defData_commitData data;
			data.pData = pFlLightDataBlock;
			data.rows = numRows;
			data.a = ( bShadow && bCookie ) ? 1 : 0;
			data.b = ( bShadow && !bCookie ) ? 1 : 0;
			data.c = ( !bShadow && bCookie ) ? 1 : 0;
			data.d = bAdvanced ? 0 : 1;
			QUEUE_FIRE( defData_commitData, Fire, data );

			if ( bCookie )
			{
				defData_Cookie data;
				data.index = 0;
				data.pCookie = l->GetCookieForDraw();
				Assert( data.pCookie != NULL );
				QUEUE_FIRE( defData_Cookie, Fire, data );

				iDrawnCookied++;
			}

			if ( !bShadow && !bCookie )
				iDrawnSimple++;

			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->MatrixMode( MATERIAL_MODEL );
			pRenderContext->PushMatrix();
			pRenderContext->LoadMatrix( l->worldTransform );

			pRenderContext->Bind( lightTypes[i].pMatPassWorld );
			l->pMesh_World->Draw();

			if ( bVolumetrics )
			{
				Assert( m_bDrawVolumetrics );
				Assert( l->pMesh_Volumetrics != NULL );

				defData_Volume data;
				data.mData.iDataOffset = 0;
				data.mData.iSamplerOffset = 0;
				data.mData.iNumRows = lightTypes[i].constCount_advanced;
				data.mData.bHasCookie = bCookie;
				QUEUE_FIRE( defData_Volume, Fire, data );

				DrawVolumePrepass( false, view, l );

				pRenderContext->PushRenderTargetAndViewport( pVolumBuffer0 );

				pRenderContext->Bind( lightTypes[i].pMatVolumeWorld );
				l->pMesh_Volumetrics->Draw();

				pRenderContext->PopRenderTargetAndViewport();

				iPassesVolumetrics[0]++;
			}

			pRenderContext->MatrixMode( MATERIAL_MODEL );
			pRenderContext->PopMatrix();
			pRenderContext.SafeRelease();
		}
		FOR_EACH_VEC_FAST_END
	}

	if ( deferred_lightmanager_debug.GetBool() )
	{
		Assert( iNumLightTypes == 2 );

		const char *pszFilterName = "UNKNOWN FILTER";
		const char *pszProfile = "HARDWARE";

#if SHADOWMAPPING_USE_COLOR
		pszProfile = "SOFTWARE";
#endif

		switch ( SHADOWMAPPING_METHOD )
		{
		case SHADOWMAPPING_DEPTH_COLOR__RAW:
		case SHADOWMAPPING_DEPTH_STENCIL__RAW:
			pszFilterName = "RAW";
			break;
		case SHADOWMAPPING_DEPTH_STENCIL__3X3_GAUSSIAN:
			pszFilterName = "3x3 GAUSS";
			break;
		case SHADOWMAPPING_DEPTH_COLOR__4X4_SOFTWARE_BILINEAR_BOX:
			pszFilterName = "4x4 BOX";
			break;
		case SHADOWMAPPING_DEPTH_COLOR__4X4_SOFTWARE_BILINEAR_GAUSSIAN:
			pszFilterName = "4x4 GAUSS";
			break;
		case SHADOWMAPPING_DEPTH_COLOR__5X5_SOFTWARE_BILINEAR_GAUSSIAN:
		case SHADOWMAPPING_DEPTH_STENCIL__5X5_GAUSSIAN:
			pszFilterName = "5x5 GAUSS";
			break;
		default:
			Assert(0); // add filter name to this switch case
		}

		engine->Con_NPrintf( 17, "STATS - RENDERING" );
		engine->Con_NPrintf( 18, "Fullscreen passes - point: %i, spot: %i, total: %i",
			iPassesDrawnFullscreen[0], iPassesDrawnFullscreen[1],
			iPassesDrawnFullscreen[0] + iPassesDrawnFullscreen[1] );
		engine->Con_NPrintf( 19, "Volumetric passes - world: %i, fullscreen: %i",
			iPassesVolumetrics[0], iPassesVolumetrics[1] );
		engine->Con_NPrintf( 20, "Stats drawn - simple: %i, shadows: %i, cookies: %i", iDrawnSimple, iDrawnShadowed, iDrawnCookied );
		engine->Con_NPrintf( 22, "Shadow mapping filter profile: %s - %s", pszProfile, pszFilterName );
	}
}

void CLightingManager::RenderVolumetrics( const CViewSetup &view )
{
	if ( !m_bDrawVolumetrics )
		return;

	ITexture *pVolumBuffer0 = GetDefRT_VolumetricsBuffer( 0 );
	ITexture *pVolumBuffer1 = GetDefRT_VolumetricsBuffer( 1 );

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->PushRenderTargetAndViewport( pVolumBuffer1 );
	pRenderContext->DrawScreenSpaceRectangle( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_BLUR_G6_X ),
		0, 0, view.width / 4, view.height / 4,
		0, 0, view.width / 4 - 1, view.height / 4 - 1,
		view.width / 4, view.height / 4 );
	pRenderContext->PopRenderTargetAndViewport();

	pRenderContext->PushRenderTargetAndViewport( pVolumBuffer0 );
	pRenderContext->DrawScreenSpaceRectangle( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_BLUR_G6_Y ),
		0, 0, view.width / 4, view.height / 4,
		0, 0, view.width / 4 - 1, view.height / 4 - 1,
		view.width / 4, view.height / 4 );
	pRenderContext->PopRenderTargetAndViewport();

	pRenderContext->DrawScreenSpaceRectangle( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_VOLUME_BLEND ),
		0, 0, view.width, view.height,
		0, 0, view.width - 1, view.height - 1,
		view.width, view.height );
}

#include "deferred/vgui/vgui_deferred.h"

void CLightingManager::DoSceneDebug()
{
	if ( !deferred_lightmanager_debug.GetBool() )
		return;

#if DEBUG
	if ( deferred_lightmanager_debug.GetInt() >= 2 )
	{
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->ClearBuffers( false, true );

		DebugLights_Draw_DebugMeshes();
	}
#endif

	engine->Con_NPrintf( 10, "Total deferred lights: %i", m_hDeferredLights.Count() );
	engine->Con_NPrintf( 11, "lights rendered: %i", m_hRenderLights.Count() );
	engine->Con_NPrintf( 13, "STATS - SORTING" );
	engine->Con_NPrintf( 14, "lights point - world: %i, fullscreen: %i",
		m_hPreSortedLights[ LSORT_POINT_WORLD ].Count(), m_hPreSortedLights[ LSORT_POINT_FULLSCREEN ].Count() );
	engine->Con_NPrintf( 15, "lights spot - world: %i, fullscreen: %i",
		m_hPreSortedLights[ LSORT_SPOT_WORLD ].Count(), m_hPreSortedLights[ LSORT_SPOT_FULLSCREEN ].Count() );
}

void CLightingManager::DebugLights_Draw_Boundingboxes()
{
	FOR_EACH_VEC_FAST( def_light_t*, m_hDeferredLights, l )
	{
		const bool bVisible = m_hRenderLights.HasElement( l );

		Color cFace = bVisible ? Color( 32, 196, 64, 16 ) : Color( 196, 64, 32, 16 );
		Color cEdge = Color( cFace[0] * 0.5f, cFace[1] * 0.5f, cFace[2] * 0.5f, 255.0f );

		debugoverlay->AddBoxOverlay2( vec3_origin,
			l->bounds_min,
			l->bounds_max,
			vec3_angle,
			cFace, cEdge, -1.0f );

		debugoverlay->AddBoxOverlay2( vec3_origin,
			l->bounds_min_naive,
			l->bounds_max_naive,
			vec3_angle,
			Color( 32, 64, 96, 16 ),
			Color( 32, 64, 96, 255 ), -1.0f );
	}
	FOR_EACH_VEC_FAST_END
}

#if DEBUG
void CLightingManager::DebugLights_Draw_DebugMeshes()
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_WIREFRAME_DEBUG ) );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();

	FOR_EACH_VEC_FAST( def_light_t*, m_hRenderLights, l )
	{
		if ( !l->pMesh_Debug )
			continue;

		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->LoadMatrix( l->worldTransform );

		l->pMesh_Debug->Draw();

		if ( l->pMesh_Debug_Volumetrics )
			l->pMesh_Debug_Volumetrics->Draw();
	}
	FOR_EACH_VEC_FAST_END

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
}
#endif