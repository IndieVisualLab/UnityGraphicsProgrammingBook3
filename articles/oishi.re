
= ScreenSpaceFluidRendering

== はじめに


本章では、パーティクルのレンダリング手法の一つとして、@<strong>{Deferred Shading}による@<strong>{Screen Space Fluid Rendering}を紹介します。


== Screen Space Fluid Renderingとは


流体のような連続体をレンダリングする手法としては、伝統的にはマーチンキューブス法が用いられますが、比較的計算負荷が高く、リアルタイムアプリケーションにおいての細部を追求した描画には向いていません。
そこで、高速にパーティクルベースの流体を描画する手法として @<strong>{Screen Space Fluid Rendering} という手法が考案されました。



//image[ssfr_screen_space_fluid_rendering_overview][Screen Space Fluid Renderingの概略図]{
//}

これは図3.1のように、カメラから見えるスクリーンスペース上のパーティクルの表面の深度からサーフェスを生成するというものです。



このサーフェスジオメトリの生成を行うために、@<strong>{Deferred Rendering}という技術を用います。


== Deferred Rendering（遅延シェーディング, 遅延レンダリング） とは


@<strong>{2次元のスクリーンスペース（画面空間）}で@<strong>{シェーディング（陰影計算）}を行う技術です。
区別のため、従来のタイプの手法は、@<strong>{Forward Rendering}と呼ばれます。



図3.2は、従来の@<strong>{Forward Rendering}と@<strong>{Deferred Rendering}のレンダリングパイプラインの概略を描いた図です。
//image[ssfr_rendering_pipeline_compare][Foward RenderingとDeferred Renderingのパイプラインの比較]{
//}




@<strong>{Forward Rendering}の場合、シェーダの@<strong>{第1パス}でライティングやシェーディング処理を行いますが、@<strong>{Deferred Rendering} では、シェーディングに必要な@<strong>{法線}、@<strong>{位置}、@<strong>{深度}、@<strong>{拡散色}などの2次元画像情報を生成し、@<strong>{G-Buffer}と呼ばれるバッファに格納します。第2パスではそれらの情報を用い、ライティング、シェーディングを行い、最終的なレンダリング結果を得ます。このように実際のレンダリングが@<strong>{第2パス（以降）}に遅延されるので、@<strong>{"Deferred（遅延）" Rendering}と呼ばれます。



@<strong>{Deferred Rendering}の利点としては、

 * 光源を多く使用できる
 * シェーディングの際に、表示されている領域だけを計算するだけで済むので、フラグメントシェーダが実行される回数を最小限に抑えることができる
 * ジオメトリの変形が可能
 * G−Bufferの情報をPostEffectなどで使用できる（SSAOなど）



欠点としては、

 * 半透明の表現に弱い
 * MSAAなどアルゴリズムによってはアンチエイリアシングの十分な効果が得られなくなる
 * 複数のマテリアルを使うのが困難
 * Orthographicカメラでのサポートをしていない



などがあり、トレードオフとなるような制約もできてしまうため、使用にはそれらを考慮した上で判断を行う必要があります。


==== Deferred Rendering の Unityでの使用条件


@<strong>{Deferred Rendering}には以下のような使用条件があり、環境によっては、本サンプルプログラムは動作いたしません。。

 * Unity Pro でのみ使用可能
 * マルチレンダーターゲット（MRT）が有効である
 * シェーダーモデル 3.0以上
 * デプスレンダーテクスチャとtwo-sidedステンシルバッファをサポートするグラフィックスカードが必要



また、@<strong>{Deferred Rendering}は、@<strong>{Orthographic}プロジェクションを使用している場合はサポートされず、カメラのプロジェクションモードが@<strong>{Orthographic}に設定されている場合は、@<strong>{Forward Rendering}が使用されます。


== G-Buffer（ジオメトリバッファ）


陰影計算、ラインティングの計算に使用する@<strong>{法線}、@<strong>{位置}、@<strong>{拡散反射色}などのスクリーンスペースでの（2次元テクスチャ）の情報は、@<strong>{G-Buffer}と呼ばれます。
Unityのレンダリングパイプラインの@<strong>{G-Bufferパス}では、それぞれのオブジェクトは一度レンダリングされ、@<strong>{G-Bufferテクスチャ}にレンダリングされ、デフォルトで以下の情報が生成されます。

//table[tbl1][]{
render target	format	datat type
-----------------
RT0	ARGB32	Diffuse color (RGB), Occulusion (A)
RT1	ARGB32	Specular color (RGB), Roughness (A)
RT2	ARGB2101010	World space normal (RGB)
RT3	ARGB2101010	Emission + (Ambient + Reflections + Lightmaps)
Z-buffer	.	Depth + Stencil
//}


これらの@<strong>{G-Bufferテクスチャ}は、グローバルなプロパティとして設定されており、シェーダ内で取得することができます。

//table[tbl2][]{
shader property name	data type
-----------------
_CameraGBufferTexture0	Diffuse color (RGB), occulusion (A)
_CameraGBufferTexture1	Specular color (RGB)
_CameraGBufferTexture2	World space normal (RGB)
_CameraGBufferTexture3	Emission + (Ambient + Reflections + Lightmaps)
_CameraDepthTexture	Depth + Stencil
//}


サンプルコードの、
@<strong>{Assets/ScreenSpaceFluidRendering/Scenes/ShowGBufferTest}
を開くと、この@<strong>{G-Buffer}を取得して画面に表示する様子を確認することができます。



//image[ssfr_g-buffer][デフォルトで生成されるG-Buffer]{
//}



== CommandBufferについて


この章で紹介するサンプルプログラムは、@<strong>{CommandBuffer}というUnityのAPIを使用します。



@<strong>{CPU}が実行するスクリプトに書かれたメソッドの中では、描画処理自体は行われません。代わりに、@<strong>{グラフィックスコマンドバッファ}と言われる@<strong>{GPU}が理解できる@<strong>{レンダリングコマンド}のリストに追加され、生成された@<strong>{コマンドバッファ}は@<strong>{GPU}によって直接読み込まれ、実行されることによって実際にオブジェクトを描画します。



Unityが用意する@<strong>{レンダリングコマンド}は、例えば、@<strong>{Graphics.DrawMesh()}, @<strong>{Graphics.DrawProcedural()}などのメソッドです。



UnityのAPIの@<strong>{CommandBuffer}を用いることで、Unityの@<strong>{レンダリングパイプライン}の特定のポイントに@<strong>{コマンドバッファ（レンダリングコマンドのリスト）}を差し込んで、
Unityの@<strong>{レンダリングパイプライン}を拡張することができます。



@<strong>{CommandBuffer}を使ったサンプルプロジェクトは、ここでいくつか確認することができます。



@<href>{https://docs.unity3d.com/ja/current/Manual/GraphicsCommandBuffers.html,https://docs.unity3d.com/ja/current/Manual/GraphicsCommandBuffers.html}


== 座標系、座標変換について


以降、スクリーンスペース上で行われる計算の内容の理解のために、簡単に、3DCGのグラフィクスパイプラインと座標系について説明いたします。


==== Homogeneous Coordinates（同次座標系）


3次元の位置ベクトル(x,y,z)を考える時に、
(x,y,z,w) というように4次元のものとして扱うことがあり、これをHomogeneous Coordinates（同次座標）と呼びます。このように4次元で考えることによって4x4のMatrix（行列）を効果的に掛け合わせることができます。座標変換の計算は、基本的に4x4のMatrixを掛け合わせることで行われるので、位置ベクトルは、このように4次元で表現します。



同時座標と、非同次座標の変換はこのように行われます。
(x/w, y/w, z/w, 1) = (x, y, z, w)


==== Object Space (オブジェクト座標系, ローカル座標系, モデル座標系)


オブジェクトそれ自身が中心となる座標系です。


==== World Space（ワールド座標系, グローバル座標系）


@<strong>{World Space} は、シーンを中心として、シーンの中で複数のオブジェクトが空間的にどのような関係にあるのかを示す座標系です。World Spaceには、オブジェクトの移動・回転・スケールを行う@<strong>{Modeling Transform}によって、@<strong>{Object Space}から変換されます。


==== Eye(View) Space（視点座標系, カメラ座標系）


@<strong>{Eye Space}は、描画するカメラを中心とし、その視点を原点とする座標系です。
カメラの位置・カメラの上方向の向き、カメラのフォーカス方向の向きなどの情報を定義した@<strong>{View Matrix}による@<strong>{View Transform}を行うことによって@<strong>{World Space}から変換されます。


==== Clip Space（クリッピング座標系, クリップ座標系）


@<strong>{Clip Space}は、上記の@<strong>{View Matrix}で定義されたカメラの他のパラメータ、field of view(FOV)・アスペクト比・near clip・far clipを定義した@<strong>{Projection Matrix}を@<strong>{View Space}に掛け合わせる変換によって得られる座標系です。この変換を@<strong>{Projection Transform}と言い、これによって、カメラで描画される空間のクリッピングを行います。


==== Normalized Device Coordinates（正規化デバイス座標系）


@<strong>{Clip Space}によって得られた座標値xyz各要素に対してwで除算することによって、-1<=x<=1、-1<=y<=1、0<=z<=1にの範囲にすべての位置座標が正規化されます。これによって得られる座標系を@<strong>{Normalized Device Coordinates（NDC）}と言います。この変換は@<strong>{Persepective Devide}と呼ばれ、手前のオブジェクトは大きく、奥のものは小さく描画されるようになります。


==== Screen(Window) Space （スクリーン座標系、ウィンドウ座標系）


@<strong>{Normalized Device Coordinates}で得られた正規化された値を、スクリーンの解像度に合うように変換した座標系です。Direct3Dの場合、左上を原点とします。



@<strong>{Deferred Rendering}では、このスクリーン空間での画像を元に計算しますが、必要によって、それぞれの変換の逆行列を掛けることによって、任意の座標系の情報を算出し使用するので、このレンダリングパイプラインを理解しておく事は重要です。



図3.3は3DCGのグラフィックスパイプラインと座標系、座標変換の関係について説明したものです。



//image[ssfr_coordinate_system_and_transform][座標系、座標変換のフロー]{
//}



== 実装についての解説


サンプルコードの、



@<strong>{Assets/ScreenSpaceFluidRendering/Scenes/ScreenSpaceFluidRendering}



シーンを開いてください。


=== アルゴリズムの概要


@<strong>{Screen Space Fluid Rendering}の大まかなアルゴリズムは以下の通りです。

 1. パーティクルを描画しスクリーンスペースの深度画像を生成する
 1. 深度画像にブラーエフェクトをかけ滑らかにする
 1. 深度から表面の法線を計算する
 1. 表面の陰影を計算する



※このサンプルコードでは、サーフェスジオメトリの作成までを行います。透過表現などは行っておリません。


=== スクリプトの構成
//table[tbl3][]{
スクリプト名	機能
-----------------
ScreenSpaceFluidRenderer.cs	メインのスクリプト
RenderParticleDepth.shader	パーティクルのスクリーンスペースの深度を求める
BilateralFilterBlur.shader	深度に応じて減衰するブラー エフェクト
CalcNormal.shader	スクリーンスペースのデプス情報から法線を求める
RenderGBuffer.shader	G-Bufferに深度、法線、カラー情報などを書き込む
//}

=== CommandBufferを作成し、カメラに登録


@<strong>{ScreenSpaceFluidRendering.cs}の@<strong>{OnWillRenderObject}関数内では、@<strong>{CommandBuffer}を作成し、カメラのレンダリングパスの任意の箇所に@<strong>{CommandBuffer}を登録する処理を行います。



以下、コードを抜粋します

//emlist[ScreenSpaceFluidRendering.cs][csharp]{

// アタッチされたレンダラー(MeshRenderer)がカメラに映っているときに呼び出される
void OnWillRenderObject()
{
  // 自身がアクティブでなければ解放処理をして、以降は何もしない
  var act = gameObject.activeInHierarchy && enabled;
  if (!act)
  {
    CleanUp();
    return;
  }
  // 現在レンダリング処理をしているカメラがなければ、以降は何もしない
  var cam = Camera.current;
  if (!cam)
  {
    return;
  }

  // 現在レンダリング処理をしているカメラに、
  // CommandBufferがアタッチされていなければ
  if (!_cameras.ContainsKey(cam))
  {  
    // CommandBufferの情報作成
    var buf = new CmdBufferInfo();
    buf.pass   = CameraEvent.BeforeGBuffer;
    buf.buffer = new CommandBuffer();
    buf.name = "Screen Space Fluid Renderer";
    // G-Bufferが生成される前のカメラのレンダリングパイプライン上のパスに、
    // 作成したCommandBufferを追加
    cam.AddCommandBuffer(buf.pass, buf.buffer);

    // CommandBufferを追加したカメラを管理するリストにカメラを追加
    _cameras.Add(cam, buf);
  }
//}


@<strong>{Camera.AddCommandBuffer(CameraEvent evt, Rendering.CommandBuffer buffer)}メソッドはカメラに、任意のパスで実行される@<strong>{コマンドバッファ}を追加します。ここでは、@<strong>{CameraEvent.BeforeGBuffer}で@<strong>{G-Bufferが生成される直前}の位置を指定しており、ここに任意の@<strong>{コマンドバッファ}を差し込むことで、スクリーンスペース上で計算されたジオメトリを生成させることができます。
追加した@<strong>{コマンドバッファ}は、アプリケーションの実行や、オブジェクトをDisableにしたタイミングで @<strong>{RemoveCommandBuffer}メソッドを用いて削除します。カメラから@<strong>{コマンドバッファ}を削除する処理は、@<strong>{Cleanup}関数内に実装しています。



続いて、@<strong>{CommandBuffer}に@<strong>{レンダリングコマンド}を登録していきます。その際、フレームの更新の頭で、@<strong>{CommandBuffer.Clear}メソッドによって、すべてのバッファのコマンドを削除しておきます。


=== パーティクルの深度画像を生成する


与えられたパーティクルの@<strong>{頂点のデータ}をもとに@<strong>{ポイントスプライト}を生成し、その@<strong>{スクリーンスペースでの深度テクスチャ}を計算します。



以下、コードを抜粋します。


//emlist[ScreenSpaceFluidRendering.cs][csharp]{

// --------------------------------------------------------------------
// 1. パーティクルをポイントスプライトとして描画し、深度とカラーのデータを得る
// --------------------------------------------------------------------
// デプスバッファのシェーダプロパティIDを取得
int depthBufferId = Shader.PropertyToID("_DepthBuffer");
// 一時的なRenderTextureを取得
buf.GetTemporaryRT(depthBufferId, -1, -1, 24, 
  FilterMode.Point, RenderTextureFormat.RFloat);

// カラーバッファとデプスバッファをレンダーターゲットに指定
buf.SetRenderTarget
(
  new RenderTargetIdentifier(depthBufferId),  // デプス
  new RenderTargetIdentifier(depthBufferId)   // デプス書き込み用
);
// カラーバッファとデプスバッファをクリア
buf.ClearRenderTarget(true, true, Color.clear);

// パーティクルのサイズをセット
_renderParticleDepthMaterial.SetFloat ("_ParticleSize", _particleSize);
// パーティクルのデータ（ComputeBuffer）をセット
_renderParticleDepthMaterial.SetBuffer("_ParticleDataBuffer", 
_particleControllerScript.GetParticleDataBuffer());

// パーティクルをポイントスプライトとして描画し、深度画像を得る
buf.DrawProcedural
(
  Matrix4x4.identity,
  _renderParticleDepthMaterial,
  0,
  MeshTopology.Points,
  _particleControllerScript.GetMaxParticleNum()
);

//}

//emlist[RenderParticleDepth.shader][hlsl]{
// --------------------------------------------------------------------
// Vertex Shader
// --------------------------------------------------------------------
v2g vert(uint id : SV_VertexID)
{
  v2g o = (v2g)0;
  FluidParticle fp = _ParticleDataBuffer[id];
  o.position = float4(fp.position, 1.0);
  return o;
}

// --------------------------------------------------------------------
// Geometry Shader
// --------------------------------------------------------------------
// ポイントスプライトの各頂点の位置
static const float3 g_positions[4] =
{
  float3(-1, 1, 0),
  float3( 1, 1, 0),
  float3(-1,-1, 0),
  float3( 1,-1, 0),
};
// 各頂点のUV座標値
static const float2 g_texcoords[4] =
{
  float2(0, 1),
  float2(1, 1),
  float2(0, 0),
  float2(1, 0),
};

[maxvertexcount(4)]
void geom(point v2g In[1], inout TriangleStream<g2f> SpriteStream)
{
  g2f o = (g2f)0;
  // ポイントスプライトの中心の頂点の位置
  float3 vertpos = In[0].position.xyz;
  // ポイントスプライト4点
  [unroll]
  for (int i = 0; i < 4; i++)
  {
    // クリップ座標系でのポイントスプライトの位置を求め代入
    float3 pos = g_positions[i] * _ParticleSize;
    pos = mul(unity_CameraToWorld, pos) + vertpos;
    o.position = UnityObjectToClipPos(float4(pos, 1.0));
    // ポイントスプライトの頂点のUV座標を代入
    o.uv       = g_texcoords[i];
    // 視点座標系でのポイントスプライトの位置を求め代入
    o.vpos     = UnityObjectToViewPos(float4(pos, 1.0)).xyz * float3(1, 1, 1);
    // ポイントスプライトのサイズを代入
    o.size     = _ParticleSize;

    SpriteStream.Append(o);
  }
  SpriteStream.RestartStrip();
}

// --------------------------------------------------------------------
// Fragment Shader
// --------------------------------------------------------------------
struct fragmentOut
{
  float  depthBuffer  : SV_Target0;
  float  depthStencil : SV_Depth;
};

fragmentOut frag(g2f i)
{
  // 法線を計算
  float3 N = (float3)0;
  N.xy = i.uv.xy * 2.0 - 1.0;
  float radius_sq = dot(N.xy, N.xy);
  if (radius_sq > 1.0) discard;
  N.z = sqrt(1.0 - radius_sq);

  // クリップ空間でのピクセルの位置
  float4 pixelPos     = float4(i.vpos.xyz + N * i.size, 1.0);
  float4 clipSpacePos = mul(UNITY_MATRIX_P, pixelPos);
  // 深度
  float  depth = clipSpacePos.z / clipSpacePos.w; // 正規化

  fragmentOut o  = (fragmentOut)0;
  o.depthBuffer  = depth;
  o.depthStencil = depth;

  return o;
}

//}


C#スクリプトでは、まず、スクリーンスペースでの計算のための@<strong>{一時的なRenderTexture}を生成します。
コマンドバッファでは、@<strong>{CommandBuffer.GetTemporaryRT}メソッドによって、@<strong>{一時的なRenderTexture}データを作り、それを利用します。@<strong>{GetTemporaryRT}メソッドの第一引数には、作成したいバッファのシェーダプロパティの@<strong>{ユニークID}を渡します。シェーダにおける@<strong>{ユニークID}とは、Unityのゲームシーンが実行される度に生成されるシェーダ内のプロパティにアクセスするための@<strong>{int型の固有のID}で、@<strong>{Shader.PropertyToID}メソッドで@<strong>{プロパティ名}を渡して生成することができます。（この固有IDは、実行されたタイミングが異なるゲームシーンでは異なるため、その値を保持したり、ネットワークを通じて他のアプリケーション共有する事はできません）



@<strong>{GetTemporaryRT}メソッドの第2,3引数では、解像度を指定します。@<strong>{-1}を指定すると、現在ゲームシーンでレンダリングしているカメラの解像度（Camera pixel width, height）が渡されます。



第4引数では、デプスバッファのビット数を指定します。@<strong>{_DepthBuffer}では、デプス+ステンシルの値も書き込みたいため、0以上の値を指定します。



生成したRenderTextureは、@<strong>{CommandBuffer.SetRenderTarget}メソッドで、@<strong>{レンダーターゲット}に指定し、@<strong>{ClearRenderTarget}
メソッドで、クリアをしておきます。これを行わないと毎フレームごとに上書きされていくため、適切に描画されません。



@<strong>{CommandBuffer.DrawProcedural}メソッドで、パーティクルのデータを描画し、@<strong>{スクリーンスペース上でのカラーと深度のテクスチャ}を計算します。
この計算を図示すると以下のようになります。



//image[ssfr_calculate_depth_texture][深度画像の計算]{
//}




@<strong>{Vertex}シェーダ、@<strong>{Geometry}シェーダでは、与えられたパーティクルのデータから視点方向にビルボードする@<strong>{ポイントスプライト}を生成します。
@<strong>{Fragment}シェーダでは、@<strong>{ポイントスプライト}のUV座標値から半球体の法線を計算し、これを元に、@<strong>{スクリーンスペースでの深度画像}を得ます。



//image[ssfr_depth_texture][深度画像]{
//}



=== 深度画像にブラーエフェクトをかけ滑らかにする


@<strong>{得られた深度画像}をブラーエフェクトをかけ滑らかにすることで、隣接するパーティクルとの境界を曖昧にし連結しているように描画することができます。
ここでは、深度に応じてブラーエフェクトのオフセット量の減衰がなされるようなフィルタを用いています。



//image[ssfr_blurred_depth_texture][ブラーをかけた深度画像]{
//}



=== 深度から表面の法線を計算する


@<strong>{ブラーを施した深度画像}から@<strong>{法線}を計算します。
法線の計算には、XとY方向に、@<strong>{偏微分}を行うことによって求めます。



//image[ssfr_calculate_normal][法線の計算]{
//}




以下、コードを抜粋します


//emlist[CalcNormal.shader][hlsl]{

// --------------------------------------------------------------------
// Fragment Shader
// --------------------------------------------------------------------
// スクリーンのUVから視点座標系での位置を求める
float3 uvToEye(float2 uv, float z)
{
  float2 xyPos = uv * 2.0 - 1.0;
  // クリップ座標系での位置
  float4 clipPos = float4(xyPos.xy, z, 1.0);
  // 視点座標系での位置
  float4 viewPos = mul(unity_CameraInvProjection, clipPos);
  // 正規化
  viewPos.xyz = viewPos.xyz / viewPos.w;

  return viewPos.xyz;
}

// 深度の値を深度バッファから得る
float sampleDepth(float2 uv)
{
#if UNITY_REVERSED_Z
  return 1.0 - tex2D(_DepthBuffer, uv).r;
#else
  return tex2D(_DepthBuffer, uv).r;
#endif
}

// 視点座標系での位置を得る
float3 getEyePos(float2 uv)
{
  return uvToEye(uv, sampleDepth(uv));
}

float4 frag(v2f_img i) : SV_Target
{
  // スクリーン座標からテクスチャのUV座標に変換
  float2 uv = i.uv.xy;
  // 深度を取得
  float depth = tex2D(_DepthBuffer, uv);

  // 深度が書き込まれていなければピクセルを破棄
#if UNITY_REVERSED_Z
  if (Linear01Depth(depth) > 1.0 - 1e-3)
    discard;
#else
  if (Linear01Depth(depth) < 1e-3)
    discard;
#endif
  // テクセルサイズを格納
  float2 ts = _DepthBuffer_TexelSize.xy;

  // 視点座標系（カメラから見た）位置をスクリーンのuv座標から求める
  float3 posEye = getEyePos(uv);

  // xについて偏微分
  float3 ddx  = getEyePos(uv + float2(ts.x, 0.0)) - posEye;
  float3 ddx2 = posEye - getEyePos(uv - float2(ts.x, 0.0));
  ddx = abs(ddx.z) > abs(ddx2.z) ? ddx2 : ddx;

  // yについて偏微分
  float3 ddy = getEyePos(uv + float2(0.0, ts.y)) - posEye;
  float3 ddy2 = posEye - getEyePos(uv - float2(0.0, ts.y));
  ddy = abs(ddy.z) > abs(ddy2.z) ? ddy2 : ddy;

  // 外積から上で求めたベクトルと直交する法線を求める
  float3 N = normalize(cross(ddx, ddy));

  // 法線をカメラの位置との関係で変更する
  float4x4 vm = _ViewMatrix;
  N = normalize(mul(vm, float4(N, 0.0)));

  // (-1.0～1.0) を (0.0～1.0) に変換 
  float4 col = float4(N * 0.5 + 0.5, 1.0);

  return col;
}

//}


//image[ssfr_normal_texture][法線画像]{
//}



=== 表面の陰影を計算する


これまでの計算で求めた深度画像と法線画像を、@<strong>{G-Buffer}に書き込みを行います。@<strong>{G-Buffer}が生成される@<strong>{レンダリングパス}の直前に書き込むことによって、計算結果を元にしたジオメトリが生成され、シェーディングやライティングが施されます。



コードを抜粋します


//emlist[ScreenSpaceFluidRendering.cs][csharp]{

// --------------------------------------------------------------------
// 4. 計算結果を G-Buffer に書き込みパーティクルを描画
// --------------------------------------------------------------------
buf.SetGlobalTexture("_NormalBuffer", normalBufferId); // 法線バッファをセット
buf.SetGlobalTexture("_DepthBuffer",  depthBufferId);  // デプスバッファをセット

// プロパティをセット
_renderGBufferMaterial.SetColor("_Diffuse",  _diffuse );
_renderGBufferMaterial.SetColor("_Specular", 
  new Vector4(_specular.r, _specular.g, _specular.b, 1.0f - _roughness));
_renderGBufferMaterial.SetColor("_Emission", _emission);

// G-Buffer を レンダーターゲットにセット
buf.SetRenderTarget
(
  new RenderTargetIdentifier[4]
  {
    BuiltinRenderTextureType.GBuffer0, // Diffuse
    BuiltinRenderTextureType.GBuffer1, // Specular + Roughness
    BuiltinRenderTextureType.GBuffer2, // World Normal
    BuiltinRenderTextureType.GBuffer3  // Emission
  },
  BuiltinRenderTextureType.CameraTarget  // Depth
);
// G-Bufferに書き込み
buf.DrawMesh(quad, Matrix4x4.identity, _renderGBufferMaterial);

//}


//emlist[RenderGBuffer.shader][hlsl]{
  
// GBufferの構造体
struct gbufferOut
{
  half4 diffuse  : SV_Target0; // 拡散反射
  half4 specular : SV_Target1; // 鏡面反射
  half4 normal   : SV_Target2; // 法線
  half4 emission : SV_Target3; // 放射光
  float depth    : SV_Depth;   // 深度
};

sampler2D _DepthBuffer; // 深度
sampler2D _NormalBuffer;// 法線

fixed4 _Diffuse;  // 拡散反射光の色
fixed4 _Specular; // 鏡面反射光の色
float4 _Emission; // 放射光の色

void frag(v2f i, out gbufferOut o)
{
  float2 uv = i.screenPos.xy * 0.5 + 0.5;

  float  d = tex2D(_DepthBuffer,  uv).r;
  float3 n = tex2D(_NormalBuffer, uv).xyz;

#if UNITY_REVERSED_Z
  if (Linear01Depth(d) > 1.0 - 1e-3)
    discard;
#else
  if (Linear01Depth(d) < 1e-3)
    discard;
#endif

  o.diffuse  = _Diffuse;
  o.specular = _Specular;
  o.normal   = float4(n.xyz , 1.0);

  o.emission = _Emission;
#ifndef UNITY_HDR_ON
  o.emission = exp2(-o.emission);
#endif

  o.depth    = d;
}
//}


@<strong>{SetRenderTarget}メソッドで、レンダーターゲットに書き込み対象の@<strong>{G-Buffer}を指定します。第1引数のターゲットとしたいカラーバッファに@<strong>{BuiltinRenderTextureType}列挙型の@<strong>{GBuffer0}、@<strong>{GBuffer1}、@<strong>{GBuffer2}、@<strong>{GBuffer3}を指定した@<strong>{RenderTargetIdentifier}の配列、また第2引数のターゲットとしたいデプスバッファに@<strong>{CameraTarget}を指定することで、@<strong>{デフォルトのG-Buffer一式}、@<strong>{深度情報}を@<strong>{レンダーターゲット}に指定することができます。



@<strong>{コマンドバッファ}で複数の@<strong>{レンダーターゲット}を指定したシェーダによるスクリーンスペース上の計算を行うために、ここでは、@<strong>{DrawMesh}メソッドを用いて、画面を覆う矩形のメッシュを描画することで、これを実現しています。


=== 一時的なRenderTextureの解放


@<strong>{GetTemporaryRT}メソッドで生成した一時的なRenderTextureは、@<strong>{ReleaseTemporaryRT}メソッドで忘れずに解放します。これを行わないと、毎フレームメモリが確保され、メモリのオーバーフローが起きてしまいます。


=== レンダリング結果


//image[ssfr_result][レンダリング結果]{
//}



== まとめ


この章は、@<strong>{Deferred Shading}によるジオメトリの操作という点にフォーカスを当てた解説でした。
今回のサンプルでは、半透明のオブジェクトとして、厚みに応じた光の吸収や内部での屈折を考慮した背景の透過、集光現象などの要素は実装しておりません。液体としての表現を追求するならば、これらの要素も実装していくと良いでしょう。
@<strong>{Deferred Rendering}を活用していくためには、Unityを使っていて普段は意識しなくても済むような座標系や座標変換、シェーディング、ライティングなどの3DCGのレンダリングにおける計算の理解が求められます。
観測の範囲では、まだまだUnityでの@<strong>{Deferred Rendering}を使用したサンプルコードや学習のためのリファレンスが多くなく、自分自身まだ理解が不十分ですが、CG表現の幅を広げることができる技術であると感じています。本章を通して、従来の@<strong>{Forward Rendering}では実現できないようなCGによる表現を行いたいという目的を持った方への一助となれば幸いです。


== 参照
 * GDC Screen Space Fluid Rendering for Games, Simon Green, NVIDIA



@<href>{http://developer.download.nvidia.com/presentations/2010/gdc/Direct3D_Effects.pdf,http://developer.download.nvidia.com/presentations/2010/gdc/Direct3D_Effects.pdf}

 * なぜなにリアルタイムレンダリング, Satoshi Kodaira



@<href>{https://www.slideshare.net/SatoshiKodaira/ss-69311865,https://www.slideshare.net/SatoshiKodaira/ss-69311865}

