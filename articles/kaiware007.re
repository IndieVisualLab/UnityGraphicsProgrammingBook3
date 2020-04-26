= Reaction Diffusion

== はじめに

自然界には、熱帯魚の横縞模様やサンゴの迷路のようなシワなどさまざまな模様があります。それらの自然界に存在する模様の発生を数式で表したのが、かの天才数学者アラン・チューリングです。彼が導き出した数式で生成される模様のことを「チューリング・パターン」といいます。一般的にこの数式は反応拡散方程式と呼ばれています。この反応拡散方程式をもとに、Unity上でComputeShaderを使って生物の模様のような絵を作るプログラムを開発します。最初は２次元平面上で動作するプログラムを作りますが、最後におまけとして３次元空間上で動作するプログラムも紹介します。
ComputeShaderについては、UnityGraphicsProgramming vol.1の「第2章 ComputeShader入門」を参照してください。

本章のサンプルは@<br>{}
@<href>{https://github.com/IndieVisualLab/UnityGraphicsProgramming3}@<br>{}
の「ReactionDiffusion」です。

== Reaction Diffusionとは
Reaction Diffusion（反応拡散系）とは、その名のとおり、空間内に分布された一種あるいは複数種の物質の濃度が、物質が互いに変化し合うような局所的な化学反応（Reaction）と、空間全体に広がる拡散（Diffusion）の、２つのプロセスの影響によって変化する様子を数理モデル化したものです。今回反応拡散方程式として「Gray-Scottモデル」を採用します。Gray-Scottモデルは、1983年にP.GrayとS.K.Scottに論文で発表されました。ざっくり説明すると、UとVの2つの仮想物質がグリッドの中に満たされた状態で、お互いに反応しあって増減したり、拡散したりすることで、時間とともに空間内の濃度が変化していくことでさまざまなパターンが現れます。

@<img>{kaiware/description}は、Gray-Scottモデルの「反応（Reaction）」の概要図です。

//image[kaiware/description][Gray-Scottモデルの「反応（Reaction）」の概要図]{
//}

 1. Uは、一定の割合で空間内に補充（Feed）されます
 1. Vが２つある時、Uと反応（Reaction）してもう１つVを作り出します
 1. このままではVが増え続けてしまうので、一定の割合でVを削除（Kill）します

また、 @<img>{kaiware/diffusion} のように、UとVはそれぞれ異なる速さで隣のグリッドに拡散していきます。

//image[kaiware/diffusion][Gray-Scottモデルの「拡散（Diffusion）」の概要図]{
//}

この拡散の速度の差によってUとVの濃度の差が生まれ、パターンが生成されます。
これらのUとVの反応と拡散は、次の方程式で表されます。

//texequation{
\frac{\partial u}{\partial t} = Du \Delta u - uv^2 + f_{(1-u)}
//}
//texequation{
\frac{\partial v}{\partial t} = Dv \Delta v + uv^2 - (f_{}+k)
//}


この式では、Uは@<m>{u}、Vは@<m>{v}で表しています。式は大きく３つに分かれています。 @<br>{}
最初の@<m>{Du \Delta u}と@<m>{Dv \Delta v}は拡散項といい、前半の@<m>{Du}と@<m>{Dv}は、@<m>{u}と@<m>{v}の拡散の速度の定数です。後半の@<m>{\Delta u}と@<m>{\Delta v}はラプラシアンといって、UとVの周囲との濃度差を無くす方向に拡散（Diffusion）する過程を表しています。 @<br>{}
２番目は反応項といい、@<m>{uv^2}はU１つとV２つで反応（Reaction）することで、Uが減り、Vが増えることを表しています。 @<br>{}
３番目の@<m>{+f_{(1-u)\}}は流入項といい、Uが減った場合に補充（Feed）される量を表しており、０に近いほど多く補充され、１に近いほど補充されなくなります。@<m>{-(f_{\}+k)}は流出項といい、増えたVを一定数減らす（Kill）ことを表しています。

もうちょっと簡単にまとめると、U１つとV２つで反応してUは減り、Vは増えていきます。このままではUは減り続け、Vは増え続ける一方なので、Uは@<m>{+f_{(1-u)\}}の分だけ補充され、Vは@<m>{-(f_{\}+k)}の分だけ強制的に減るようになっています。そして、UとVは@<m>{Du \Delta u}と@<m>{Dv \Delta v}によって周囲に拡散していきます。

== Unityでの実装

なんとなく方程式の雰囲気がわかったところでUnityでの実装の説明に移ります。
動作確認できるサンプルシーンは、@<b>{ReactionDiffusion2D_1}です。

=== グリッド構造体の定義
２次元の平面空間のグリッドの中に、UとVのそれぞれの濃度の値が入っていると仮定します。
今回はComputeShaderを使って並列に処理するため、ComputeBufferでグリッドを管理します。
まず、１グリッドの中の構造体を定義します。
//emlist[ReactionDiffusion2D.cs]{
public struct RDData
{
    public float u; // Uの濃度
    public float v; // Vの濃度
}
//}

=== 初期化
//emlist[ReactionDiffusion2D.cs]{
/// <summary>
/// 初期化
/// </summary>
void Initialize()
{
    ...

    int wh = texWidth * texHeight;  // バッファのサイズ
    buffers = new ComputeBuffer[2]; // ダブルバッファリング用のComputeBufferの配列初期化

    for (int i = 0; i < buffers.Length; i++)
    {
        // グリッドの初期化
        buffers[i] = new ComputeBuffer(wh, Marshal.SizeOf(typeof(RDData)));
    }

    // リセット用のグリッド配列
    bufData = new RDData[wh];
    bufData2 = new RDData[wh];

    // バッファの初期化
    ResetBuffer();

    // Seed追加用バッファの初期化
    inputData = new Vector2[inputMax];
    inputIndex = 0;
    inputBuffer = new ComputeBuffer(
        inputMax, Marshal.SizeOf(typeof(Vector2))
      );
}
//}
更新用であるComputeBufferの@<b>{buffers}は２次元配列ですが、これは読み込み用と書き込み用に分けるために２つ用意しています。というのも、ComputeShaderはマルチスレッドで並列に処理されています。今回のように周囲のグリッドを参照して計算結果が変わる処理をする場合、１つのバッファだと、処理するスレッドの順番によって先に計算し終わったグリッドの値を参照したりして計算結果がかわってきてしまいます。それを防ぐために、読み込み用と書き込み用の２つに分けています。

=== 更新処理

//emlist[ReactionDiffusion2D.cs]{
// 更新処理
void UpdateBuffer()
{
    cs.SetInt("_TexWidth", texWidth);
    cs.SetInt("_TexHeight", texHeight);
    cs.SetFloat("_DU", du);
    cs.SetFloat("_DV", dv);

    cs.SetFloat("_Feed", feed);
    cs.SetFloat("_K", kill);

    cs.SetBuffer(kernelUpdate, "_BufferRead", buffers[0]);
    cs.SetBuffer(kernelUpdate, "_BufferWrite", buffers[1]);
    cs.Dispatch(kernelUpdate,
      Mathf.CeilToInt((float)texWidth / THREAD_NUM_X),
      Mathf.CeilToInt((float)texHeight / THREAD_NUM_X),
      1);

    SwapBuffer();
}
//}
C#側のソースでは、前述の方程式にもあったパラメータをComputeShaderに渡して更新処理を行っています。
次に、ComputeShader内の更新処理について説明します。
//emlist[ReactionDiffusion2D.compute]{
// 更新処理
[numthreads(THREAD_NUM_X, THREAD_NUM_X, 1)]
void Update(uint3 id : SV_DispatchThreadID)
{

  int idx = GetIndex(id.x, id.y);
  float u = _BufferRead[idx].u;
  float v = _BufferRead[idx].v;
  float uvv = u * v * v;
  float f, k;

  f = _Feed;
  k = _K;

  _BufferWrite[idx].u = saturate(
      u + (_DU * LaplaceU(id.x, id.y) - uvv + f * (1.0 - u))
    );
  _BufferWrite[idx].v = saturate(
      v + (_DV * LaplaceV(id.x, id.y) + uvv - (k + f) * v)
    );
}
//}
まさに前述の方程式と同様の計算を行っています。GetIndex()は、２次元のグリッド座標と１次元のComputeBufferのインデックスを紐付けるための関数です。

//emlist[ReactionDiffusion2D.compute]{
// バッファのインデックス計算
int GetIndex(int x, int y) {
  x = (x < 0) ? x + _TexWidth : x;
  x = (x >= _TexWidth) ? x - _TexWidth : x;

  y = (y < 0) ? y + _TexHeight : y;
  y = (y >= _TexHeight) ? y - _TexHeight : y;

  return y * _TexWidth + x;
}
//}
_BufferReadには１フレーム前の計算結果が入っています。そこからuとvを取り出します。
LaplaceUとLaplaceVは、自分のグリッドの周囲８マスのUとVの濃度を集めるラプラシアン関数です。これによって周囲のグリッドと濃度が平均化されていきます。斜めのグリッドは影響度は低くなるように調整しています。
//emlist[ReactionDiffusion2D.compute]{
// Uのラプラシアン関数
float LaplaceU(int x, int y) {
  float sumU = 0;

  for (int i = 0; i < 9; i++) {
    int2 pos = laplaceIndex[i];
    int idx = GetIndex(x + pos.x, y + pos.y);
    sumU += _BufferRead[idx].u * laplacePower[i];
  }

  return sumU;
}

// Vのラプラシアン関数
float LaplaceV(int x, int y) {
  float sumV = 0;

  for (int i = 0; i < 9; i++) {
    int2 pos = laplaceIndex[i];
    int idx = GetIndex(x + pos.x, y + pos.y);
    sumV += _BufferRead[idx].v * laplacePower[i];
  }

  return sumV;
}
//}
uとvを計算したら、_BufferWriteに書き込みます。saturateは0～1の間でクリップするための保険です。

=== 入力処理
AキーやCキーを押すことで、グリッドに意図的にUとVの濃度差を追加する機能を用意しています。Aキーを押すことでランダムな位置にSeedNum個の点（Seed）を配置します。Cキーを押すことで中心に１つ点を配置します。

//emlist[ReactionDiffusion2D.cs]{
/// <summary>
/// Seedの追加
/// </summary>
/// <param name="x"></param>
/// <param name="y"></param>
void AddSeed(int x, int y)
{
  if (inputIndex < inputMax)
  {
    inputData[inputIndex].x = x;
    inputData[inputIndex].y = y;
    inputIndex++;
  }
}
//}
inputData配列にグリッド上の点の座標を格納しています。

//emlist[ReactionDiffusion2D.cs]{
/// <summary>
/// Seed配列をComputeShaderにわたす
/// </summary>
void AddSeedBuffer()
{
  if (inputIndex > 0)
  {
    inputBuffer.SetData(inputData);
    cs.SetInt("_InputNum", inputIndex);
    cs.SetInt("_TexWidth", texWidth);
    cs.SetInt("_TexHeight", texHeight);
    cs.SetInt("_SeedSize", seedSize);
    cs.SetBuffer(kernelAddSeed, "_InputBufferRead", inputBuffer);
    cs.SetBuffer(kernelAddSeed, "_BufferWrite", buffers[0]);    // update前なので0
    cs.Dispatch(kernelAddSeed,
      Mathf.CeilToInt((float)inputIndex / (float)THREAD_NUM_X),
      1,
      1);
    inputIndex = 0;
  }
}
//}
@<b>{inputBuffer}に、さきほどの点の座標が入ったinputeData配列をセットして、ComputeShaderに渡しています。

//emlist[ReactionDiffusion2D.compute]{
// シードの追加
[numthreads(THREAD_NUM_X, 1, 1)]
void AddSeed(uint id : SV_DispatchThreadID)
{
  if (_InputNum <= id) return;

  int w = _SeedSize;
  int h = _SeedSize;
  float radius = _SeedSize * 0.5;

  int centerX = _InputBufferRead[id].x;
  int centerY = _InputBufferRead[id].y;
  int startX = _InputBufferRead[id].x - w / 2;
  int startY = _InputBufferRead[id].y - h / 2;
  for (int x = 0; x < w; x++)
  {
    for (int y = 0; y < h; y++)
    {
      float dis = distance(
        float2(centerX, centerY),
        float2(startX + x, startY + y)
      );
      if (dis <= radius) {
        _BufferWrite[GetIndex((centerX + x), (centerY + y))].v = 1;
      }
    }
  }
}
//}
C#から渡されたinputBufferの座標を中心に円形になるようにvの値を1にしています。

=== RenderTextureに結果を書き込み
更新したグリッドはただの配列なので、可視化のためにRenderTextureに書き込んで画像にします。RenderTextureにはuとvの濃度差を書き込みます。 @<br>{}
まずはRenderTextureを作成します。
1ピクセルに書き込む情報は濃度差だけなので、RenderTextureFormatはRFloatにしておきます。RenderTextureFormat.RFloatは1ピクセルに付きfloat１つ分の情報が書き込めるRenderTextureの形式です。

//emlist[ReactionDiffusion2D.cs]{
/// <summary>
/// RenderTexture作成
/// </summary>
/// <param name="width"></param>
/// <param name="height"></param>
/// <returns></returns>
RenderTexture CreateRenderTexture(int width, int height)
{
  RenderTexture tex = new RenderTexture(width, height, 0,
    RenderTextureFormat.RFloat,
    RenderTextureReadWrite.Linear);
  tex.enableRandomWrite = true;
  tex.filterMode = FilterMode.Bilinear;
  tex.wrapMode = TextureWrapMode.Repeat;
  tex.Create();

  return tex;
}
//}

続いてComputeShaderにRenderTextureを渡して書き込むC#側の処理です。
//emlist[ReactionDiffusion2D.cs]{
/// <summary>
/// ReactionDiffusionの結果をテクスチャに書き込み
/// </summary>
void DrawTexture()
{
  cs.SetInt("_TexWidth", texWidth);
  cs.SetInt("_TexHeight", texHeight);
  cs.SetBuffer(kernelDraw, "_BufferRead", buffers[0]);
  cs.SetTexture(kernelDraw, "_HeightMap", resultTexture);
  cs.Dispatch(kernelDraw,
    Mathf.CeilToInt((float)texWidth / THREAD_NUM_X),
    Mathf.CeilToInt((float)texHeight / THREAD_NUM_X),
    1);
}
//}

こちらは、ComputeShader側の処理です、グリッドのバッファからuとvの濃度差を求めて、テクスチャに書き込んでいます。
//emlist[ReactionDiffusion2D.compute]{
// テクスチャ書き込み用の値計算
float GetValue(int x, int y) {
  int idx = GetIndex(x, y);
  float u = _BufferRead[idx].u;
  float v = _BufferRead[idx].v;
  return 1 - clamp(u - v, 0, 1);
}

...

// テクスチャに描画
[numthreads(THREAD_NUM_X, THREAD_NUM_X, 1)]
void Draw(uint3 id : SV_DispatchThreadID)
{
  float c = GetValue(id.x, id.y);

  // height map
  _HeightMap[id.xy] = c;

}
//}

=== 描画

通常のUnlit Shaderを改修して、前項で作ったテクスチャの明度をもとに、２色を間を補間しています。
//emlist[ReactionDiffusion2D.cs]{
/// <summary>
/// マテリアルの更新
/// </summary>
void UpdateMaterial()
{
  material.SetTexture("_MainTex", resultTexture);

  material.SetColor("_Color0", bottomColor);
  material.SetColor("_Color1", topColor);
}
//}

//emlist[ReactionDiffusion2D.shader]{
fixed4 frag (v2f i) : SV_Target
{
  // sample the texture
  fixed4 col = lerp(_Color0, _Color1, tex2D(_MainTex, i.uv).r);
  return col;
}
//}

実行すると画面上に生物のような模様が広がっていくはずです。
//image[kaiware/rd2d_1][シミュレーションの様子][scale=0.5]{
//}

== パラメータを変えてみる
FeedとKillのパラメータを少し変えてみるだけで、さまざまな模様が浮かび上がります。
ここでいくつかパラメータの組み合わせを紹介します。

//pagebreak

=== サンゴのような模様
 Feed:0.037 / Kill:0.06

//image[kaiware/coral][サンゴのような模様][scale=0.40]{
//}

=== つぶつぶ模様
 Feed:0.03 / Kill:0.062

//image[kaiware/points001][つぶつぶ模様][scale=0.40]{
//}

//pagebreak

=== つぶつぶが消滅と分裂を繰り返す模様
 Feed:0.0263 / Kill:0.06

//image[kaiware/points002][つぶつぶが消滅と分裂を繰り返す模様][scale=0.40]{
//}

=== まっすぐ伸びて、ぶつからないように避ける模様
 Feed:0.077 / Kill:0.0615

//image[kaiware/worm][まっすぐ伸びて、ぶつからないように避ける模様][scale=0.40]{
//}

//pagebreak

=== ぷつぷつ穴模様
 Feed:0.039 / Kill:0.058

//image[kaiware/hole][ぷつぷつ穴模様][scale=0.40]{
//}

=== 常にうねうねして安定しない模様
 Feed:0.026 / Kill:0.051

//image[kaiware/chaos][常にうねうねして安定しない模様][scale=0.40]{
//}

//pagebreak

=== 波紋のように広がり続ける模様
 Feed:0.014 / Kill:0.0477

//image[kaiware/wave][波紋のように広がり続ける模様][scale=0.40]{
//}

== おまけ：Surface Shader対応版

ここで、Surface Shaderを利用したUnityならではのきれいな質感表現をしたサンプルを紹介します。
動作確認できるサンプルシーンは、@<b>{ReactionDiffusion2D_2}です。

=== 通常版との変更点

ReactionDiffusionの処理自体は通常版と同じですが、描画用のテクスチャの作成時に、立体感を出すためのノーマルマップも作成しています。
また、結果のテクスチャはRenderTextureFormat.RFloatでしたが、ノーマルマップはXYZ方向のノーマルベクトルを格納するため、RenderTextureFormat.ARGBFloatで作成しています。

//emlist[ReactionDiffusion2DForStandard.cs]{
void Initialize()
{
  ...
  heightMapTexture = CreateRenderTexture(texWidth, texHeight,
    RenderTextureFormat.RFloat);        // 高さマップ用テクスチャ作成
  normalMapTexture = CreateRenderTexture(texWidth, texHeight,
    RenderTextureFormat.ARGBFloat);     // ノーマルマップマップ用テクスチャ作成
  ...
}

/// <summary>
/// RenderTexture作成
/// </summary>
/// <param name="width"></param>
/// <param name="height"></param>
/// <param name="texFormat"></param>
/// <returns></returns>
RenderTexture CreateRenderTexture(
  int width,
  int height,
  RenderTextureFormat texFormat)
{
    RenderTexture tex = new RenderTexture(width, height, 0,
      texFormat, RenderTextureReadWrite.Linear);
    tex.enableRandomWrite = true;
    tex.filterMode = FilterMode.Bilinear;
    tex.wrapMode = TextureWrapMode.Repeat;
    tex.Create();

    return tex;
}

...

void DrawTexture()
{
  ...
  cs.SetTexture(kernelDraw, "_HeightMap", heightMapTexture);
  cs.SetTexture(kernelDraw, "_NormalMap", normalMapTexture);  // ノーマルマップ用テクスチャセット
  cs.Dispatch(kernelDraw,
    Mathf.CeilToInt((float)texWidth / THREAD_NUM_X),
    Mathf.CeilToInt((float)texHeight / THREAD_NUM_X),
    1);
}
//}

ComputeShader内では、周囲のグリッドとの濃度差から傾きを求めてノーマルマップ用のテクスチャに書き込んでいます。

//emlist[ReactionDiffusion2DStandard.compute]{
float3 GetNormal(int x, int y) {
  float3 normal = float3(0, 0, 0);
  float c = GetValue(x, y);
  normal.x = ((GetValue(x - 1, y) - c) - (GetValue(x + 1, y) - c));
  normal.y = ((GetValue(x, y - 1) - c) - (GetValue(x, y + 1) - c));
  normal.z = 1;
  normal = normalize(normal) * 0.5 + 0.5;
  return normal;
}

...

// テクスチャに描画
[numthreads(THREAD_NUM_X, THREAD_NUM_X, 1)]
void Draw(uint3 id : SV_DispatchThreadID)
{
  float c = GetValue(id.x, id.y);

  // height map
  _HeightMap[id.xy] = c;

  // normal map
  _NormalMap[id.xy] = float4(GetNormal(id.x, id.y), 1);
}
//}

作成した２枚のテクスチャをSurface Shaderに渡して模様を描画します。
Surface Shaderは、Unityの物理ベースレンダリングを簡単に使えるようにラッピングされたシェーダーで、surf関数の中で@<b>{SurfaceOutputStandard}構造体に必要なデータを代入して出力するだけで、自動的にライティングしてくれます。

//emlist[SurfaceOutputStandard構造体の定義]{
struct SurfaceOutputStandard
{
    fixed3 Albedo;      // ベース (ディフューズかスペキュラー) カラー
    fixed3 Normal;      // 法線
    half3 Emission;     // 発光色
    half Metallic;      // 0=非メタル, 1=メタル
    half Smoothness;    // 0=粗い, 1=滑らか
    half Occlusion;     // オクルージョン (デフォルト 1)
    fixed Alpha;        // 透明度のアルファ
};
//}

//emlist[ReactionDiffusion2DStandard.shader]{
void surf(Input IN, inout SurfaceOutputStandard o) {

  float2 uv = IN.uv_MainTex;

  // 濃度取得
  half v0 = tex2D(_MainTex, uv).x;

  // 法線取得
  float3 norm = UnpackNormal(tex2D(_NormalTex, uv));

  // AとBの境界の値を出す
  half p = smoothstep(_Threshold, _Threshold + _Fading, v0);

  o.Albedo = lerp(_Color0.rgb, _Color1.rgb, p);         // ベース色
  o.Alpha = lerp(_Color0.a, _Color1.a, p);              // アルファ値
  o.Smoothness = lerp(_Smoothness0, _Smoothness1, p);   // スムースネス
  o.Metallic = lerp(_Metallic0, _Metallic1, p);         // メタリック
  o.Normal = normalize(float3(norm.x, norm.y, 1 - _NormalStrength));  // 法線

  o.Emission = lerp(_Emit0 * _EmitInt0, _Emit1 * _EmitInt1, p).rgb;   // 発光
}
//}

Unityのビルトイン関数の@<b>{unpackNormal関数}を使ってノーマルマップから法線を取得します。また、濃度差の割合から@<b>{SurfaceOutputStandard}の各種色や質感を設定しています。

実行すると次のような模様ができ上がるはずです。
//image[kaiware/rd_surface_shader][SurfaceShader版]{
//}
ノーマルマップによって、立体感が生まれています。また、モノクロではわかりませんが、シーン上のRGB３色のポイントライトの光沢も表現されています。

== ３次元への拡張

今まで２次元の平面上でのシミュレーションだったReaction Diffusionを３次元に拡張してみましょう。
基本的な流れは２次元のときと同じですが、次元が１つ増えているのでRenderTextureやComputeBufferの作り方、ラプラス演算の仕方が少し変わっています。
動作確認できるサンプルシーンは、@<b>{ReactionDiffusion3D}です。

=== バッファの初期化部分

濃度差の書き込み先のRenderTextureを２次元から３次元にするため、初期化処理をいくつか追加しています。

//emlist[ReactionDiffusion3D.cs]{
RenderTexture CreateTexture(int width, int height, int depth)
{
  RenderTexture tex = new RenderTexture(width, height, 0,
    RenderTextureFormat.RFloat, RenderTextureReadWrite.Linear);
  tex.volumeDepth = depth;
  tex.enableRandomWrite = true;
  tex.dimension = UnityEngine.Rendering.TextureDimension.Tex3D;
  tex.filterMode = FilterMode.Bilinear;
  tex.wrapMode = TextureWrapMode.Repeat;
  tex.Create();

  return tex;
}
//}

まず、tex.volumeDepthにＺ方向の深さを入れています。それから、tex.dimensionにUnityEngine.Rendering.TextureDimension.Tex3Dを入れています。これは、RenderTextureが３次元のボリュームテクスチャであることを指定するための設定です。これでRenderTextureが３次元のボリュームテクスチャになりました。
同様にReaction Diffusionのシミュレーション結果を格納するComputeBufferも３次元化します。こちらは単純に幅×高さ×深さのサイズを確保するだけです。

//emlist[ReactionDiffusion3D.cs]{
void Initialize()
{
  ...
  int whd = texWidth * texHeight * texDepth;
  buffers = new ComputeBuffer[2];
  ...
  for (int i = 0; i < buffers.Length; i++)
  {
     buffers[i] = new ComputeBuffer(whd, Marshal.SizeOf(typeof(RDData)));
  }
  ...
}
//}

=== シミュレーションの３次元化

続いてComputeShader側の変更点です。まず、結果の書き込み用のRenderTextureが３次元になったため、ComputeShader側のテクスチャの定義が@<b>{RWTexture2D<float>}から@<b>{RWTexture3D<float>}に変わります。

//emlist[ReactionDiffusion3D.compute]{
  RWTexture3D<float> _HeightMap;	// ハイトマップ
//}

次にラプラシアン関数の３次元化です。３×３×３の合計２７マスを参照するように変更しています。ちなみにlaplacePowerの影響度はなんとなくで割り出した値です。

//emlist[ReactionDiffusion3D.compute]{
// ラプラシアン関数で参照する周囲のインデックス計算用テーブル
static const int3 laplaceIndex[27] = {
  int3(-1,-1,-1), int3(0,-1,-1), int3( 1,-1,-1),
  int3(-1, 0,-1), int3(0, 0,-1), int3(1, 0,-1),
  int3(-1, 1,-1), int3(0, 1,-1), int3(1, 1,-1),

  int3(-1,-1, 0), int3(0,-1, 0), int3(1,-1, 0),
  int3(-1, 0, 0), int3(0, 0, 0), int3(1, 0, 0),
  int3(-1, 1, 0), int3(0, 1, 0), int3(1, 1, 0),

  int3(-1,-1, 1), int3(0,-1, 1), int3(1,-1, 1),
  int3(-1, 0, 1), int3(0, 0, 1), int3(1, 0, 1),
  int3(-1, 1, 1), int3(0, 1, 1), int3(1, 1, 1),
};

// ラプラシアンの周囲のグリッドの影響度
static const float laplacePower[27] = {
  0.02, 0.02, 0.02,
  0.02, 0.1,  0.02,
  0.02, 0.02, 0.02,

  0.02, 0.1,  0.02,
  0.1, -1.0,  0.1,
  0.02, 0.1,  0.02,

  0.02, 0.02, 0.02,
  0.02, 0.1,  0.02,
  0.02, 0.02, 0.02
};

// バッファのインデックス計算
int GetIndex(int x, int y, int z) {
  x = (x < 0) ? x + _TexWidth : x;
  x = (x >= _TexWidth) ? x - _TexWidth : x;

  y = (y < 0) ? y + _TexHeight : y;
  y = (y >= _TexHeight) ? y - _TexHeight : y;

  z = (z < 0) ? z + _TexDepth : z;
  z = (z >= _TexDepth) ? z - _TexDepth : z;

  return z * _TexWidth * _TexHeight + y * _TexWidth + x;
}

// Uのラプラシアン関数
float LaplaceU(int x, int y, int z) {
  float sumU = 0;

  for (int i = 0; i < 27; i++) {
    int3 pos = laplaceIndex[i];

    int idx = GetIndex(x + pos.x, y + pos.y, z + pos.z);
    sumU += _BufferRead[idx].u * laplacePower[i];
  }
  return sumU;
}

// Vのラプラシアン関数
float LaplaceV(int x, int y, int z) {
  float sumV = 0;

  for (int i = 0; i < 27; i++) {
    int3 pos = laplaceIndex[i];
    int idx = GetIndex(x + pos.x, y + pos.y, z + pos.z);
    sumV += _BufferRead[idx].v * laplacePower[i];
  }
  return sumV;
}
//}

=== 描画処理

シミュレーション結果のRenderTextureが３次元のボリュームテクスチャになっているので、今までのようにUnlit ShaderやSurface Shaderにテクスチャを貼り付けても正常に表示されません。
サンプルではマーチングキューブス法という手法を使ってポリゴンを生成して描画していますが、紙面の都合上、実装についての説明は省略させていただきます。マーチングキューブス法の解説については、申し訳ありませんがUnity Graphics Programming Vol.1の「第７章 雰囲気で始めるマーチングキューブス法入門」を参照してください。
他にも、レイマーチングを使ったボリュームレンダリングで描画する方法もあります。凹さんのブログに非常にわかりやすい実装@<fn>{kw_hecomi}が紹介されているので、ぜひとも参考にしてください。

//footnote[kw_hecomi][凹みTips http://tips.hecomi.com/entry/2018/01/05/192332]

//image[kaiware/rd3d][３次元版Reaction Diffusion]{
//}

== まとめ

Gray-Scottモデルを使って生物のような模様を作る方法を紹介しました。FeedとKillのパラメータを少し変えるだけで全然違う模様ができるので、夢中になるとあっという間に時間が過ぎてしまうので注意しましょう（※個人差があります） @<br>{}
また、Reaction Diffusionを使った作品には、Nakama Kouheiさんの「DIFFUSION」@<fn>{kw_diffusion}やKitahara Nobutakaさんの「Reaction-Diffusion」@<fn>{kw_reaction_diffusion}があります。みなさんもReaction Diffusionの不思議な魅力に取り憑かれてみませんか？

//footnote[kw_diffusion][DIFFUSION https://vimeo.com/145251635]
//footnote[kw_reaction_diffusion][Reaction-Diffusion https://vimeo.com/176261480]

== 参考

 * Reaction-Diffusion Tutorial http://www.karlsims.com/rd.html
 * Reaction diffusion system（Gray-Scott model） @<br>{}https://pmneila.github.io/jsexp/grayscott/
