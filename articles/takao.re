= Gravitational N-Body Simulation

== はじめに
本章では、宇宙空間に存在している天体の動きをシミュレートする方法である、Gravitational N-Body Simulationの、GPU実装手法を解説します。

//image[takao/thumb][Result]

該当サンプルプログラムは、@<br>{}
@<href>{https://github.com/IndieVisualLab/UnityGraphicsProgramming3}@<br>{}
「Assets/NBodySimulation」になります。

== N-Bodyシミュレーションとは
N個の物理的物体の相互作用を計算するシミュレーションのことを、N-Bodyシミュレーションと総称します。
N-Bodyシミュレーションを用いる問題には多数の種類があり、特に、宇宙空間に点在している天体同士が重力によって引き合い、まとまりを形成する系を扱う問題のことを、@<kw>{重力多体系問題}と呼びます。
本章で解説されるアルゴリズムはこれに該当し、N-Bodyシミュレーションを用いて重力多体系の運動方程式を解いていく、ということになります。

また、N-Bodyシミュレーションは、重力多体系問題以外に、

 * 分子間力の計算
 * ダークマターの解析
 * 銀河団の衝突の解析

など、小さいものから壮大なものまで、幅広い分野に応用されています。

== アルゴリズム
早速、どのような数式を解いていくのか見ていきましょう。

重力多体系問題は、高校物理を履修している方にとっては馴染み深い方程式である、@<kw>{万有引力の方程式}を、空間内の全天体に対して計算してあげることで、シミュレートが可能です。
ただし、高校物理では一直線上にある物体のみを扱う都合上、このような記述で学習されていたのではないかと思います。
//embed[latex]{
  \begin{equation}
    f = G \frac{Mm}{r^2}
  \end{equation}
//}
ここで、@<m>{f}は万有引力、@<m>{G}は万有引力定数、@<m>{M, m}は2天体のそれぞれの質量、@<m>{r}は天体間の距離です。
当然ながら、これでは2天体間の@<kw>{力の大きさ}(スカラー量)しか求めることはできません。

今回の実装では、Unity内部の3次元空間上での動きを考える必要があるため、方向を示す@<kw>{ベクトル量}が必要になります。
そこで、2天体(@<m>{i, j})間で発生する力のベクトルを求めるために、万有引力の方程式を次のように記述します。
//embed[latex]{
  \begin{equation}
    \label{eq:first}
    \mbox{\boldmath $f$}_{ij} = G \frac{m_im_j}{\|\mbox{\boldmath $r$}_{ij}\|^2} \cdot \frac{\mbox{\boldmath $r$}_{ij}}{\|\mbox{\boldmath $r$}_{ij}\|}
  \end{equation}
//}
ここで、@<m>{\mbox{\boldmath $f$\}_{ij\}}は、天体iが天体jから受ける力のベクトル、@<m>{m_i, m_j}は2天体のそれぞれの質量、@<m>{r_{ij\}}は天体@<m>{j}から天体@<m>{i}への方向ベクトルです。
右辺左側は、最初に出てきた万有引力の方程式と同じく、力の大きさを算出しており、
右辺右側部分で力を受ける方向の単位ベクトルをかけることで、ベクトル化しています。
//indepimage[takao/vec][数式の意味][scale=0.5]

さらに、2天体間ではなく、1つの天体(@<m>{i})が周囲のすべての天体から受ける力の大きさを@<m>{\mbox{\boldmath $F$\}_{i\}}とすると、次のように計算できます。
//embed[latex]{
  \begin{equation}
    \mbox{\boldmath $F$}_{i} = \sum_{j \in N} \mbox{\boldmath $f$}_{ij} = Gm_i \cdot \sum_{j \in N} \frac{m_j \mbox{\boldmath $r$}_{ij}}{\|\mbox{\boldmath $r$}_{ij}\|^3}
  \end{equation}
//}
式にあるように、すべての万有引力の総和をとることで、周囲の天体から受ける力を算出することができます。

また、シミュレーションを簡単にするために、Softening因子@<m>{\varepsilon}を用いて方程式を書き換えると、次のようになります。
//embed[latex]{
  \begin{equation}
    \mbox{\boldmath $F$}_{i} \simeq Gm_i \cdot \sum_{j \in N} \frac{m_j \mbox{\boldmath $r$}_{ij}}{\left( \|\mbox{\boldmath $r$}_{ij}\|^2 + \varepsilon \right)^{\frac{3}{2}}}
  \end{equation}
//}
これにより、天体が同一の位置に来てしまっても衝突を無視することが可能になります(自身同士で計算をしてしまってもシグマ内の結果は@<m>{0}となります)。

次に、運動の第二法則@<m>{m \mbox{\boldmath $a$\} = \mbox{\boldmath $f$\}}を利用して、力のベクトルを加速度のベクトルに変換していきます。
まず、運動の第二法則を変形して、次式のようにします。
//embed[latex]{
  \begin{equation}
    m_i \mbox{\boldmath $a$}_{i} = \mbox{\boldmath $F$}_{i} \Longrightarrow \mbox{\boldmath $a$}_{i} = \frac{\mbox{\boldmath $F$}_{i}}{m_i}
  \end{equation}
//}

続いて、重力多体系の運動方程式に、先ほどの変形式を代入して書き換えると、天体が受ける加速度を算出することができます。
//embed[latex]{
  \begin{equation}
  \label{eq:newton}
    \mbox{\boldmath $a$}_{i} \simeq G \cdot \sum_{j \in N} \frac{m_j \mbox{\boldmath $r$}_{ij}}{\left( \|\mbox{\boldmath $r$}_{ij}\|^2 + \varepsilon \right)^{\frac{3}{2}}}
  \end{equation}
//}

これで、シミュレーションの下準備は整いました。さて、数式では重力多体系問題を表現することができましたが、これらの数式をプログラムに落とし込むにはどのようにすればよいのでしょうか。
次節でしっかりと解説していきたいと思います。

== 差分法
先述の式(@<raw>{\ref{eq:newton\}})は、方程式の中でも@<kw>{微分方程式}というものに分類されます。
というのも、物理世界では@<kw>{位置、速度、加速度}の関係は次の画像のようになっており、加速度が位置関数の2階微分であることから、そのまま微分方程式と呼ばれます。

//image[takao/diff][位置・速度・加速度の関係][scale=0.5]

コンピュータで微分方程式を解く方法は様々存在しますが、中でも一般的なのが@<kw>{差分法}と呼ばれるアルゴリズムです。
手始めに、微分のおさらいから解説をはじめていきましょう。

=== 微分
まずはじめに、数学的な微分の定義を確認します。
関数@<m>{f(t)}の微分は次式で定義されます。
//embed[latex]{
  \begin{equation}
  \label{eq:delta}
    \frac{df}{dt} = \lim_{\Delta t \to 0} \frac{f(t + \Delta t) - f(t)}{\Delta t} 
  \end{equation}
//}

数式だけでは何を示しているのかがわかりにくいので、グラフに置き換えると次のようになります。

//image[takao/graph][前進差分][scale=0.6]

関数の微分値は、@<m>{t_n}時点でのグラフの傾きになることはすでにご存じかとは思います。
つまるところこのグラフは、傾きを算出するために@<m>{\Delta t}を無限に小さくしていきますよ、という状態を表しており、式(@<raw>{\ref{eq:delta\}})そのものを表していることがわかるかと思います。

=== 差分
コンピュータ上では数値としての「無限」を扱うことができません。
そのため、できる限り小さい有限の@<m>{\Delta t}で近似してあげることになります。
それを考慮して、先ほどの微分の定義を差分に書き換えると、次式のようになります。
//embed[latex]{
  \begin{equation}
  \label{eq:differenciate}
    \frac{df}{dt} \simeq \frac{f(t + \Delta t) - f(t)}{\Delta t} 
  \end{equation}
//}
そのまま極限が取れた形ですね。
「無限小の@<m>{\Delta t}はコンピュータ上で表現できないので、ある程度の大きさの@<m>{\Delta t}で止めて近似します」という認識で大丈夫だと思います。

ここで、先ほどの@<img>{takao/diff}を物理っぽく表してみると、次のようになります。

//image[takao/diff2][位置・速度・加速度の関係(数式)][scale=0.5]

つまり、式(@<raw>{\ref{eq:differenciate\}})を、@<img>{takao/diff2}に照らし合わせると、次のようになります。
//embed[latex]{
  \begin{equation}
  \label{eq:x}
    \frac{dx}{dt} \simeq \frac{x(t + \Delta t) - x(t)}{\Delta t} = v(t)
  \end{equation}
//}

//embed[latex]{
  \begin{equation}
  \label{eq:v}
    \frac{dv}{dt} \simeq \frac{v(t + \Delta t) - v(t)}{\Delta t} = a(t)
  \end{equation}
//}

さらに、式(@<raw>{\ref{eq:x\}}), (@<raw>{\ref{eq:v\}})を合成すると、次のようになります。
//embed[latex]{
  \begin{equation}
   \label{eq:res}
    x(t + \Delta t) = x(t) + v(t + \Delta t) \Delta t = x(t) + (v(t) + a(t) \Delta t) \Delta t
  \end{equation}
//}

この式は、現在時刻@<m>{t}から「@<m>{\Delta t}秒後の位置座標は、現在時刻の加速度と速度がわかっていれば算出が可能である」ということを意味しています。
これが、差分法でシミュレーションを行う際の基礎的な考え方となります。
また、このような微分方程式を差分法を用いて表現したものを、@<kw>{差分方程式(漸化式)}と呼びます。

実際に差分法でリアルタイムシミュレーションを行う場合は、微小時間@<m>{\Delta t}(タイムステップ)を、1フレームの描画時間(60fpsであれば、1/60秒)にするのが一般的です。

== 実装
では、いよいよ実装に入っていきます。
該当シーンは、「SimpleNBodySimulation.unity」になります。

=== CPU側のプログラム

==== 天体のデータ構造
まず初めに、天体粒子のデータ構造を定義します。
式(@<raw>{\ref{eq:delta\}})を見ると、1つの天体がもつべき物理量は、「位置、速度、質量」であることがわかります。
よって、次の構造体を定義してあげればよさそうです。

//emlist[Body.cs]{
public struct Body
{
  public Vector3 position;
  public Vector3 velocity;
  public float mass;
}
//}

==== バッファの用意

次に、生成したい粒子数をインスペクタから設定し、その個数分バッファを確保します。
読み込み用と書き込み用でバッファを分けることにより、データの書き込み競合が起きないようにします。

また、構造体ひとつあたりのByte数は、「System.Runtime.InteropServices」名前空間の、「Marshal.SizeOf(Type t)」関数で取得することが可能です。

//emlist[SimpleNBodySimulation.cs]{
void InitBuffer()
{
  // バッファの作成 (Read/Write用) → 競合防止
  bodyBuffers = new ComputeBuffer[2];

    // 各要素がBody構造体のバッファを粒子の個数分作成
  bodyBuffers[READ] = new ComputeBuffer(numBodies, 
    Marshal.SizeOf(typeof(Body)));

  bodyBuffers[WRITE] = new ComputeBuffer(numBodies, 
    Marshal.SizeOf(typeof(Body)));
}
//}

==== 天体の初期配置
続いて、空間に粒子を配置します。初めに、粒子用の配列を作成し、それぞれの要素に物理量の初期値を与えます。
サンプルでは、球体内をランダムサンプリングして初期位置とし、速度を0、質量をランダムに与えました。

最後に、作成した配列をバッファをセットして、準備は完了です@<fn>{scaling}。

//footnote[scaling][※ ルック調整のために、位置座標をスケーリングできる変数を用意していますが、すでに調整済みですので皆さんは気にする必要はありません。]

//emlist[SimpleNBodySimulation.cs]{
void DistributeBodies()
{
  Random.InitState(seed);

  // ルック調整用
  float scale = positionScale 
                * Mathf.Max(1, numBodies / DEFAULT_PARTICLE_NUM);

  // バッファにセットするための配列を用意
  Body[] bodies = new Body[numBodies];

  int i = 0;
  while (i < numBodies)
  {
    // 球体内でサンプリング
    Vector3 pos = Random.insideUnitSphere;

    // 配列にセット
    bodies[i].position = pos * scale;
    bodies[i].velocity = Vector3.zero;
    bodies[i].mass = Random.Range(0.1f, 1.0f);


    i++;
  }

  // バッファに配列をセット
  bodyBuffers[READ].SetData(bodies);
  bodyBuffers[WRITE].SetData(bodies);

}
//}


==== シミュレーションルーチン

いよいよ、シミュレーションを実際に動かしていきます。次のコードは、毎フレーム実行される部分になります。

まずは、ComputeShaderの定数バッファに値をセットします。
差分方程式の@<m>{\Delta t}には、Unityに用意されている「Time.deltaTime」を使用します。また、GPUの実装の都合上、スレッド数・スレッドブロック数もあわせて転送しています。

計算終了後、シミュレーションの計算結果は、書き込み用のバッファに格納されていますので、次のフレームで読み込み用バッファとして利用するために最後の行でバッファを入れ替えています。

//emlist[SimpleNBodySimulation.cs]{
void Update()
{
  // コンピュートシェーダに定数を転送
  // Δt
  NBodyCS.SetFloat("_DeltaTime", Time.deltaTime);
  // 速度減衰率
  NBodyCS.SetFloat("_Damping", damping);
  // Softening因子
  NBodyCS.SetFloat("_SofteningSquared", softeningSquared);
  // 粒子数
  NBodyCS.SetInt("_NumBodies", numBodies);

  // ブロック当たりのスレッド数
  NBodyCS.SetVector("_ThreadDim", 
    new Vector4(SIMULATION_BLOCK_SIZE, 1, 1, 0));

  // ブロック数
  NBodyCS.SetVector("_GroupDim",
  new Vector4(Mathf.CeilToInt(numBodies / SIMULATION_BLOCK_SIZE), 1, 1, 0));

  // バッファアドレスを転送
  NBodyCS.SetBuffer(0, "_BodiesBufferRead", bodyBuffers[READ]);
  NBodyCS.SetBuffer(0, "_BodiesBufferWrite", bodyBuffers[WRITE]);

  // コンピュートシェーダ実行
  NBodyCS.Dispatch(0, 
    Mathf.CeilToInt(numBodies / SIMULATION_BLOCK_SIZE), 1, 1);


  // Read/Writeを入れ替え (競合防止)
  Swap(bodyBuffers);
}
//}


==== レンダリング
シミュレーション計算後に、粒子をレンダリングするマテリアルに対してインスタンス描画指示を出します。
粒子をレンダリングする際、粒子の位置座標をシェーダに与えてあげる必要があるため、計算後のバッファをレンダリング用シェーダに転送します。

//emlist[ParticleRenderer.cs]{
void OnRenderObject()
{
  particleRenderMat.SetPass(0);
  particleRenderMat.SetBuffer("_Particles"", bodyBuffers[READ]);

  Graphics.DrawProcedural(MeshTopology.Points, numBodies);
}
//}

=== GPU側のプログラム

N-Bodyシミュレーションでは、すべての粒子との相互作用を計算する必要がありますので、
シンプルに計算していては実行時間が@<m>{O(n^2)}となってしまい、パフォーマンスを出すことができません。
そこで、UnityGraphicsProgramming Vol1、第3章に掲載されているSharedMemory(共有メモリ)の使い方を活用することにします。

==== 考え方

同一ブロック内にあるデータは、シェアードメモリに格納してしまい、I/Oを高速化します。スレッドブロックをタイルに見立てた概念図を次に示します。

//image[takao/tile][タイルの概念]

ここで、行は実行されているグローバルスレッド(DispatchThreadID)、列はスレッド内で全数調査されている対象の粒子です。
時間が経過するにつれて、実行している列が右に1個ずつずれていくような認識です。

また、同時に実行されるタイルの総数は、(粒子の個数 / グループ内スレッド数)になります。
サンプルでは、ブロック内スレッド数を256個(SIMULATION_BLOCK_SIZE)としているので、実際にはタイルの中身は5x5ではなく、256x256あるという認識です。

すべての行は並列に動いていますが、タイル内でデータを共有するため、タイル内で実行されている列がすべてSyncに到達するまで同期待ちを行います(Sync層より右に行かない)。
Sync層に到達したのち、次のタイルのデータをシェアードメモリに読み込みなおす、という形です。

==== 定数バッファの用意
CPUからの入力を受けるための定数バッファを、ComputeShader内に記述します。

また、粒子データを保存するためのバッファも用意しておきます。
今回、Body構造体は「Body.cginc」にまとめておきました。
後々使いまわしそうなものは、cgincにまとめておくと便利です。

最後に、シェアードメモリを使用するための宣言もしておきます。

//emlist[SimpleNBodySimulation.compute]{
#include "Body.cginc"

// 定数
cbuffer cb {
	float	_SofteningSquared, _DeltaTime, _Damping;
	uint	_NumBodies;
	float4	_GroupDim, _ThreadDim;
};


// 粒子のバッファ
StructuredBuffer<Body> _BodiesBufferRead;
RWStructuredBuffer<Body> _BodiesBufferWrite;

// 共有メモリ (ブロック内で共有される)
groupshared Body sharedBody[SIMULATION_BLOCK_SIZE];
//}


==== タイルの実装
次に、タイルを実装します。

//emlist[SimpleNBodySimulation.compute]{
float3 computeBodyForce(Body body, uint3 groupID, uint3 threadID)
{

  uint start = 0;				// 開始
  uint finish = _NumBodies;	
    
  float3 acc = (float3)0;
  int currentTile = 0;

  // タイル数(ブロック数)分実行
  for (uint i = start; i < finish; i += SIMULATION_BLOCK_SIZE)
  {
    // 共有メモリに格納
    // sharedBody[ブロック内スレッドID] 
    // = _BodiesBufferRead[タイルID * ブロック内総スレッド数 + スレッドID]
    sharedBody[threadID.x]
      = _BodiesBufferRead[wrap(groupID.x + currentTile, _GroupDim.x) 
        * SIMULATION_BLOCK_SIZE + threadID.x];
      
    // グループ同期
    GroupMemoryBarrierWithGroupSync();

    // 周囲からの重力の影響を計算
    acc = gravitation(body, acc, threadID);

    GroupMemoryBarrierWithGroupSync();

    currentTile++;	// 次のタイルへ
  }

  return acc;

}
//}

コード内にあるforループのイメージ図を次の画像に置いておきます。

//image[takao/tile_uni][タイルIDのforループ]


==== 相互作用の計算

先ほどのループでタイルの移動を制御していましたので、今度は@<kw>{タイルの中でのforループ}を実装します。

//emlist[SimpleNBodySimulation.compute]{
float3 gravitation(Body body, float3 accel, uint3 threadID)
{

  // 全数調査
  // ブロック内のスレッド数分実行
  for (uint i = 0; i < SIMULATION_BLOCK_SIZE;)
  {
    accel = bodyBodyInteraction(accel, sharedBody[i], body);
    i++;
  }

  return accel;
}
//}

これで、タイル内の全数調査が完了します。また、この関数のreturn後のタイミングで、タイル内のすべてのスレッドが処理を完了するまで待機します。

次に、式(@<raw>{\ref{eq:first\}})を、次のように実装します。
//emlist[SimpleNBodySimulation.compute]{
// シグマ内の計算
float3 bodyBodyInteraction(float3 acc, Body b_i, Body b_j) 
{
  float3 r = b_i.position - b_j.position;

  // distSqr = dot(r_ij, r_ij) + EPS^2
  float distSqr = r.x * r.x + r.y * r.y + r.z * r.z;
  distSqr += _SofteningSquared;

  // invDistCube = 1/distSqr^(3/2) 
  float distSixth = distSqr * distSqr * distSqr;
  float invDistCube = 1.0f / sqrt(distSixth);
  
  // s = m_j * invDistCube
  float s = b_j.mass * invDistCube;

  // a_i =  a_i + s * r_ij
  acc += r * s;

  return acc;
}
//}

これで、加速度の総和を求めるプログラムが完成しました。

==== 差分法で位置を更新

続いて、次のフレームでの粒子の座標・速度を、これまでの計算で算出された加速度を使って、算出します。

//emlist[SimpleNBodySimulation.compute]{
[numthreads(SIMULATION_BLOCK_SIZE,1,1)]
void CSMain (
	uint3 groupID : SV_GroupID,	// グループID
	uint3 threadID : SV_GroupThreadID, // グループ内スレッドID
	uint3 DTid : SV_DispatchThreadID // グローバルスレッドID
) {

	// 現在のグローバルスレッドインデックス
	uint index = DTid.x;
	
	// 粒子をバッファから読み込み
	Body body = _BodiesBufferRead[index];
	
	float3 force = computeBodyForce(body, groupID, threadID);
	
	body.velocity += force * _DeltaTime;
	body.velocity *= _Damping;
	
	// 差分法
	body.position += body.velocity * _DeltaTime;
	
	_BodiesBufferWrite[index] = body;
   
}
//}

天体の位置座標が更新できました。
これで、天体の動きのシミュレーションは完成です！

== 粒のレンダリング方法
本節では、前回@<fn>{ugp1}の記事で説明が不十分だった、
GPUパーティクルの描画方法について補足しておきたいと思います。

//footnote[ugp1][「Unity Graphics Programming Vol.1 - 第5章 SPH法による流体シミュレーション」でも、同様の粒子のレンダリングを行っています。]

=== ビルボード
ビルボードとは、常にカメラの方向を向く、シンプルな平面オブジェクトのことを指します。世の中の大半のパーティクルシステムは、ビルボードによって実装されているといっても過言ではありません。
そのビルボードをシンプルに実装するためには、ビュー変換行列をうまく使ってあげる必要があります。

ビュー変換行列には、カメラ位置と回転を、原点に戻すような数値の情報が含まれています。
つまり、ビュー変換行列を空間内のすべてのオブジェクトに掛けることで、カメラを原点とした座標系に変換されるわけです。

よって、@<kw>{カメラの方向を向く}という特徴を持ったビルボードには、回転情報のみを含んだビュー変換行列の@<kw>{逆行列}を、モデル変換行列としてかけてあげればよさそうです。(後述しますが、わかりにくいので@<img>{takao/billboard}に図解を掲載しています)

==== ビルボードの実装

まず初めに、パーティクルを描画するためのQuadメッシュを作成します。
これは、ジオメトリシェーダで1つの頂点を、xy平面に平行なQuadに拡張することで簡単に実現できます@<fn>{ugpgeo}。
//footnote[ugpgeo][ジオメトリシェーダの詳しい解説は、UnityGraphicsProgramming Vol.1 「ジオメトリシェーダで草を生やす」をご覧ください。]

//image[takao/bill][GeometryShaderによるQuad拡張][scale=0.9]

このQuadに対して、ビュー変換行列の平行移動成分を打ち消した逆行列@<fn>{inverse}を、モデル変換行列として与えてあげると、その場でカメラの方向を向くQuadが作成できます。
//footnote[inverse][ビュー変換行列の逆行列は、単純に転置すればよいことが知られているので、ここでは転置で実装しています。]

何を言っているのかわからないと思いますので、次に解説図を示します。

//image[takao/billboard][ビルボードの仕組み]

さらに、カメラの方向に向いたビルボードに対してビュー、プロジェクション行列をかけることにより、画面上の座標に変換することができます。
これらを実装したシェーダを次に示します。

//emlist[ParticleRenderer.shader]{

[maxvertexcount(4)]
void geom(point v2g input[1], inout TriangleStream<g2f> outStream) {
  g2f o;

  float4 pos = input[0].pos;

  float4x4 billboardMatrix = UNITY_MATRIX_V;
  
  // 回転成分だけ取り出す
  billboardMatrix._m03 = billboardMatrix._m13 = 
    billboardMatrix._m23 = billboardMatrix._m33 = 0;

  for (int x = 0; x < 2; x++) {
    for (int y = 0; y < 2; y++) {
      float2 uv = float2(x, y);
      o.uv = uv;

      o.pos = pos
        + mul(transpose(billboardMatrix), float4((uv * 2 - float2(1, 1))
        * _Scale, 0, 1));

      o.pos = mul(UNITY_MATRIX_VP, o.pos);

      o.id = input[0].id;

      outStream.Append(o);
    }
  }

  outStream.RestartStrip();
}

//}



== 結果
以上のシミュレーションの結果を見てみましょう。

//image[takao/simple][シミュレーション結果 (コレジャナイ感...)]

動きを見てみると、すべての粒子が中心に集まってしまっていて、視覚的に面白くありません。そこで、次節にてひと工夫加えていきます。

== 視覚的に面白くするための工夫
該当シーンは、「NBodySimulation.unity」です。
工夫といっても、変更点は1行だけで、次のようにタイル計算する部分を途中で打ち切ってしまいます。

//emlist[NBodySimulation.compute]{
float3 computeBodyForce(Body body, uint3 groupID, uint3 threadID)
{
  ・・・
	
  uint finish = _NumBodies / div;	// 途中で切る

  ・・・  
	
}
//}

これにより、相互作用の計算が全数調査されることなく途中で破棄されますが、
結果的にすべての粒子の影響を受けなかったことで、粒子の塊がいくつか発生し、1点に集約されることはなくなります。

そして、複数の塊の部分同士が相互作用することで、冒頭の@<img>{takao/thumb}ような、よりダイナミックな動きを生みます。

== まとめ
本章では、Gravitational N-Body SimulationのGPU実装手法を解説しました。
小さな原子から、大きな宇宙まで、N-Bodyシミュレーションの持つポテンシャルは無限大です。
ぜひ皆さんも、オリジナルの宇宙をUnity上で作ってみてはいかがでしょうか。
少しでもお力になれましたら、幸いです！

== 参考

 * GPU Gems 3 - Chapter 31. Fast N-Body Simulation with CUDA
 * N体シミュレーションで何がわかるか？ - 鹿児島大学 藤井通子 @<href>{http://www.astro-wakate.org/ss2011/web/ss11_proceedings/proceeding/galaxy_fujii.pdf}
 * クォータニオンとビルボード - wgld.org @<href>{https://wgld.org/d/webgl/w035.html}