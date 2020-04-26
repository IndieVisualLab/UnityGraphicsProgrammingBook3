
= GPU-Based Cellular Growth Simulation

== はじめに

Processing@<fn>{processing}による建築分野でのプロシージャルなモデリングを行うライブラリ、
iGeo@<fn>{igeo}のチュートリアルに紹介されている「Cell Division and Growth Algorithm 1」@<fn>{tutorial55}のアルゴリズムをもとに、
GPUを活用して細胞の分裂と成長の表現を行うプログラムを開発します。

本章のサンプルは@<br>{}
@<href>{https://github.com/IndieVisualLab/UnityGraphicsProgramming3}@<br>{}
の「CellularGrowth」です。

本章では、GPUでの細胞の分裂と成長プログラムを通して、

 * Append/ConsumeStructuredBufferを用いたGPUで動的にオブジェクト数を制御する方法
 * GPU上でのネットワーク構造の表現
 * Atomic演算による逐次的な処理

について解説を行います。

//footnote[processing][https://processing.org/]
//footnote[igeo][http://igeo.jp]
//footnote[tutorial55][http://igeo.jp/tutorial/55.html]

#@# メインビジュアル
//image[Nakamura/ClosedLinkDeferred][CellularGrowthSphere.scene]

まずはParticleのみのシンプルな実装を紹介した後、
Edgeを導入し、成長しながら複雑化していくネットワーク構造を表現する手法を解説します。

== 細胞の分裂と成長シミュレーション

シミュレーションプログラムでは細胞のふるまいを模倣するため、
ParticleとEdgeという2つの構造体を用意します。

1つのParticleは1つの細胞を表し、
以下のようなふるまいをします。

 * Growth(成長) : 時間とともに大きくなる
 * Repulsion(反発) : ほかのParticleと衝突して反発しあう
 * Division(分裂) : 特定の条件で分裂して2つのParticleに増える

#@# 図
//image[Nakamura/FigureGrowthRepulsionDivision][細胞のふるまい]

Edgeは細胞同士がくっつきあう様子を表現します。
分裂したParticle同士をEdgeで繋ぎ、
バネのように引き合うことでParticle同士をくっつけ、
細胞のネットワーク構造を表現します。

//image[Nakamura/FigureEdgeSpring][Edgeは繋いだParticle同士をくっつけ合う]

== 実装

本節では必要な機能を段階的に実装することを通じて解説を進めます。

=== Particleの実装 (CellularGrowthParticleOnly.cs)

まずはParticleの挙動のみを実装したサンプル
CellularGrowthParticleOnly.cs
を通して、
Particleの挙動と実装を解説します。

Particleの構造は以下のように定義します。

//emlist[Particle.cs]{
[StructLayout(LayoutKind.Sequential)]
public struct Particle_t {
    public Vector2 position;    // 位置
    public Vector2 velocity;    // 速度
    float radius;               // サイズ
    float threshold;            // 最大サイズ
    int links;                  // 繋がっているEdgeの数(後述のsceneで利用)
    uint alive;                 // 活性化フラグ
}
//}

本プロジェクトではParticleを任意のタイミングで増減させるため、
オブジェクトプールをAppend/ConsumeStructuredBufferによって管理し、
GPU上でオブジェクトの数を制御できるようにします。

==== Append/ConsumeStructuredBufferについて

Append/ConsumeStructuredBuffer@<fn>{appendstructuredbuffer}@<fn>{consumestructuredbuffer}は、Direct3D11から利用可能になったGPU上でLIFO(Last In First Out : 後入れ先出し)を行うためのコンテナです。
AppendStructuredBufferはデータの追加を行い、
ConsumeStructuredBufferはデータの取り出しを行う役割を持ちます。

このコンテナを用いることでGPU上で動的に数を制御でき、
オブジェクトの増減を表現できるようになります。

//footnote[appendstructuredbuffer][https://docs.microsoft.com/ja-jp/windows/desktop/direct3dhlsl/sm5-object-appendstructuredbuffer]
//footnote[consumestructuredbuffer][https://docs.microsoft.com/ja-jp/windows/desktop/direct3dhlsl/sm5-object-consumestructuredbuffer]

==== バッファの初期化

まずはParticleのバッファとオブジェクトプールのバッファの初期化を行います。

//emlist[CellularGrowthParticleOnly.cs]{
protected void Start () {
    // Particleの初期化
    particleBuffer = new PingPongBuffer(count, typeof(Particle_t));

    // オブジェクトプールの初期化
    poolBuffer = new ComputeBuffer(
        count, 
        Marshal.SizeOf(typeof(int)), 
        ComputeBufferType.Append
    );
    poolBuffer.SetCounterValue(0);
    countBuffer = new ComputeBuffer(
        4, 
        Marshal.SizeOf(typeof(int)), 
        ComputeBufferType.IndirectArguments
    );
    countBuffer.SetData(countArgs);

    // 分裂可能なオブジェクトを管理するオブジェクトプール
    dividablePoolBuffer = new ComputeBuffer(
        count, 
        Marshal.SizeOf(typeof(int)), 
        ComputeBufferType.Append
    );
    dividablePoolBuffer.SetCounterValue(0);

    // Particleとオブジェクトプールの初期化カーネルの実行(後述)
    InitParticlesKernel();

    ...
}
//}

particleBufferとして利用しているPingPongBufferクラスはバッファを読み込み用と書き込み用の2つを用意するもので、
後述のParticleの相互作用を計算する場面において活用します。

poolBufferとdividablePoolBufferがAppend/ConsumeStructuredBufferであり、
初期化時の引数ComputeBufferTypeにComputeBufferType.Appendを指定しています。
Append/ConsumeStructuredBufferは可変長のデータを扱えるのですが、
初期化コードを見てわかるように、
データ数の上限はバッファの作成時に設定しなければなりません。

int型のAppend/ConsumeStructuredBufferとして作成したpoolBufferは、

 1. 初期化時に非活性なParticleのindexをpoolBufferに貯める(StackへのPush)
 2. Particleを追加する際にpoolBufferからindexを取り出し(StackからのPop)、そのindexに紐づくparticleBuffer内のParticleのaliveフラグをonにする

という流れによってオブジェクトプールとして機能させます。
つまり、poolBufferが持つintバッファは常に非活性なParticleのindexを指しており、
必要に応じて取り出したりすることによってオブジェクトプールとして機能させることができるのです。(@<img>{Nakamura/ObjectPool})

#@# 図
//image[Nakamura/ObjectPool][左の配列がparticleBufferで右がpoolBufferを表している　初期状態ではparticleBuffer内のParticleが全て非活性状態だが、Particleを出現させる際はpoolBufferから非活性のParticleのindexを取り出し、該当indexの箇所のParticleを活性化させる]

countBufferはint型のバッファで、オブジェクトプールの数を管理するために用います。

Startの最後に呼び出しているInitParticlesKernelでは、
Particleとオブジェクトプールの初期化を行うGPUカーネルを実行しています。

//emlist[CellularGrowthParticleOnly.cs]{
protected void InitParticlesKernel()
{
    var kernel = compute.FindKernel("InitParticles");
    compute.SetBuffer(kernel, "_Particles", particleBuffer.Read);

    // オブジェクトプールをAppendStructuredBufferとして指定
    compute.SetBuffer(kernel, "_ParticlePoolAppend", poolBuffer);

    Dispatch1D(kernel, count);
}
//}

以下が初期化を行うカーネルになります。

//emlist[CellularGrowth.compute]{
THREAD
void InitParticles(uint3 id : SV_DispatchThreadID)
{
  uint idx = id.x;

  uint count, strides;
  _Particles.GetDimensions(count, strides);
  if (idx >= count)
    return;

  // Particleの初期化
  Particle p = create();
  p.alive = false; // 全Particleを非活性に
  _Particles[idx] = p;

  // オブジェクトプールにParticleのindexを追加
  _ParticlePoolAppend.Append(idx);
}
//}

上記のカーネルを実行することで、
particleBuffer内のすべてのParticleが初期化され非活性状態に、
poolBufferには非活性状態の全Particleのindexが格納されます。

==== Particleの出現

Particleを初期化できたので、次はParticleを出現させます。
CellularGrowthParticleOnly.csでは、マウスをクリックした位置にParticleを発生させます。

//emlist[CellularGrowthParticleOnly.cs]{
protected void Update() {
    ...
    if(Input.GetMouseButton(0))
    {
        EmitParticlesKernel(GetMousePoint());
    }
    ...
}
//}

マウスがクリックされていると、EmitParticlesKernelを実行してParticleを出現させます。

//emlist[CellularGrowthParticleOnly.cs]{
protected void EmitParticlesKernel(Vector2 point, int emitCount = 32)
{
    // オブジェクトプールの数とemitCountを比較して、
    // オブジェクトプールが空の状態で_ParticlePoolConsume.Consume()が実行されないようにする
    emitCount = Mathf.Max(
        0, 
        Mathf.Min(emitCount, CopyPoolSize(poolBuffer))
    );
    if (emitCount <= 0) return;

    var kernel = compute.FindKernel("EmitParticles");
    compute.SetBuffer(kernel, "_Particles", particleBuffer.Read);

    // オブジェクトプールをConsumeStructuredBufferとして指定
    compute.SetBuffer(kernel, "_ParticlePoolConsume", poolBuffer);

    compute.SetVector("_Point", point);
    compute.SetInt("_EmitCount", emitCount);

    Dispatch1D(kernel, emitCount);
}
//}

InitParticlesKernelでは_ParticlePoolAppendパラメータに指定していたpoolBufferを、
EmitParticlesKernelでは_ParticlePoolConsumeパラメータに指定していることからわかるように、
Append/ConsumeStructuredBufferにはそれぞれ同一のバッファを指定します。

GPU上の処理での用途によって、
バッファを追加用か(AppendStructuredBuffer)、取り出し用か(ConsumeStructuredBuffer)の
設定を変えているだけで、
CPU側からみると同じバッファをGPU側に送信していることになります。

EmitParticlesKernelの冒頭では、
emitCountとGetPoolSizeで取得したオブジェクトプールのサイズを比較していますが、
これはオブジェクトプールが空の状態でプールからのindexの取り出しが実行されないようにするためで、
もし空のオブジェクトプールからさらにindexを取り出そうとすると
(GPUカーネル内で_ParticlePoolConsume.Consumeを実行すると)、
予期しない動作が発生してしまいます。

//emlist[CellularGrowth.compute]{
THREAD
void EmitParticles(uint3 id : SV_DispatchThreadID)
{
  // _EmitCountよりも多くのParticleを追加しないようにする
  if (id.x >= (uint) _EmitCount)
    return;

  // オブジェクトプールから非活性のParticleのindexを取り出し
  uint idx = _ParticlePoolConsume.Consume();

  Particle c = create();

  // マウスの位置から少しずれた位置にParticleを配置する
  float2 offset = random_point_on_circle(id.xx + float2(0, _Time));
  c.position = _Point.xy + offset;
  c.radius = nrand(id.xx + float2(_Time, 0));

  // 活性化したParticleを非活性だったindexの個所に設定
  _Particles[idx] = c;
}
//}

EmitParticlesでは非活性なParticleのindexをオブジェクトプールから取り出し、
活性化したParticleをparticleBufferの該当indexの位置に設定しています。

上記のカーネルの処理によって、
オブジェクトプールの数を考慮しつつParticleを出現させることができます。

==== Particleのふるまい

これでParticleの出現を管理することができたので、
次はParticleのふるまいをプログラムしていきます。

本章で開発するシミュレータの細胞は@<img>{Nakamura/FigureGrowthRepulsionDivision}にもある通り、以下のふるまいをします。

 * Growth : Particleは特定のサイズに達するまで徐々に大きくなる
 * Repulsion : ほかのParticleと接触すると反発しあうように力が加わる
 * Division : Particleは特定の条件で分裂する

==== GrowthとRepulsion

GrowthとRepulsionはUpdate内で毎フレーム実行します。

//emlist[CellularGrowthParticleOnly.cs]{
protected void Update() {
    ...
    UpdateParticlesKernel();
    ...
}
...
protected void UpdateParticlesKernel()
{
    var kernel = compute.FindKernel("UpdateParticles");

    // 読み込み用のバッファを設定
    compute.SetBuffer(kernel, "_ParticlesRead", particleBuffer.Read);

    // 書き込み用のバッファを設定
    compute.SetBuffer(kernel, "_Particles", particleBuffer.Write);

    compute.SetFloat("_Drag", drag);            // 速度の減衰率
    compute.SetFloat("_Limit", limit);          // 速度の限界値
    compute.SetFloat("_Repulsion", repulsion);  // 反発する距離にかける係数
    compute.SetFloat("_Grow", grow);            // 成長速度

    Dispatch1D(kernel, count);

    // 読み込み用と書き込み用のバッファをスワップ(Ping Pong)
    particleBuffer.Swap();
}
//}

読み込み用と書き込み用のバッファをそれぞれ設定し、
処理の後にバッファをスワップしている理由は後述します。

以下がUpdateParticlesカーネルになります。

//emlist[CelluarGrowth.compute]{
THREAD
void UpdateParticles(uint3 id : SV_DispatchThreadID)
{
  uint idx = id.x;

  uint count, strides;
  _ParticlesRead.GetDimensions(count, strides);
  if (idx >= count)
    return;

  Particle p = _ParticlesRead[idx];

  // 活性化しているParticleのみ処理する
  if (p.alive)
  {
    // Grow : Particleの成長
    p.radius = min(p.threshold, p.radius + _DT * _Grow);

    // Repulsion : Particle同士の衝突
    for (uint i = 0; i < count; i++)
    {
      Particle other = _ParticlesRead[i];
      if(i == idx || !other.alive) continue;

      // Particle同士の距離を計算
      float2 dir = p.position - other.position;
      float l = length(dir);

      // Particle同士の距離が互いの半径の合計*_Repulsionよりも
      // 近ければ衝突している
      float r = (p.radius + other.radius) * _Repulsion;
      if (l < r)
      {
        p.velocity += normalize(dir) * (r - l);
      }
    }

    float2 vel = p.velocity * _DT;
    float vl = length(vel);
    // check if velocity length over than zero to avoid NaN position
    if (vl > 0)
    {
      p.position += normalize(vel) * min(vl, _Limit);

      // _Dragパラメータに従ってvelocityを減衰させる
      p.velocity = 
        normalize(p.velocity) * 
        min(
          length(p.velocity) * _Drag, 
          _Limit
        );
    }
    else
    {
      p.velocity = float2(0, 0);
    }
  }

  _Particles[idx] = p;
}
//}

UpdateParticlesカーネルでは、Particle同士の衝突を計算するため、
読み込み用のバッファ(_ParticlesRead)と書き込み用のバッファ(_Particles)とを利用しています。

もしここで読み込みも書き込みも同一のバッファを利用してしまった場合、
GPUの並列処理により、
別スレッドで更新された後のParticleの情報を、
また別のスレッドがParticleの位置計算に用いる可能性が出てきてしまい、
計算の整合性が取れない問題(データレース)が発生してしまいます。

一つのスレッドが別スレッドで更新される情報を参照しなければ、
読み込みと書き込み用で別々にバッファを用意する必要はありませんが、
スレッドが別スレッドで更新されたバッファを参照してしまうような場合は、
UpdateParticlesカーネルのように読み込みと書き込み用のバッファを別々に用意し、
更新を行うたびに交互に入れ替える必要があります。
(処理が終わるたびに交互にバッファを入れ替えることからPing Pongバッファと呼びます)

==== Division

Particleの分裂はコルーチンによって一定時間ごとに実行します。

Particleの分裂処理は

 1. 分裂可能なParticleのindexを取得し、dividablePoolBufferに格納
 2. dividablePoolBufferから分裂させたい分のParticleを取り出し、分裂させる

という流れで行われます。

//emlist[CellularGrowthParticleOnly.cs]{
protected void Start() {
    ...
    StartCoroutine(IDivider());
}

...

protected IEnumerator IDivider()
{
    yield return 0;
    while(true)
    {
        yield return new WaitForSeconds(divideInterval);
        Divide();
    }
}

protected void Divide() {
    GetDividableParticlesKernel();
    DivideParticlesKernel(maxDivideCount);
}

...

// 分裂可能なParticle候補をdividablePoolBufferに格納
protected void GetDividableParticlesKernel()
{
    // dividablePoolBufferをリセット
    dividablePoolBuffer.SetCounterValue(0);

    var kernel = compute.FindKernel("GetDividableParticles");
    compute.SetBuffer(kernel, "_Particles", particleBuffer.Read);
    compute.SetBuffer(kernel, "_DividablePoolAppend", dividablePoolBuffer);

    Dispatch1D(kernel, count);
}

protected void DivideParticlesKernel(int maxDivideCount = 16)
{
    // 分裂させたい数(maxDivideCount)と
    // 分裂可能なParticleの数(dividablePoolBufferのサイズ)を比較
    maxDivideCount = Mathf.Min(
        CopyPoolSize(dividablePoolBuffer), 
        maxDivideCount
    );

    // 分裂させたい数(maxDivideCount)と
    // オブジェクトプールに残っているParticleの数(poolBufferのサイズ)を比較
    maxDivideCount = Mathf.Min(CopyPoolSize(poolBuffer), maxDivideCount);

    if (maxDivideCount <= 0) return;

    var kernel = compute.FindKernel("DivideParticles");
    compute.SetBuffer(kernel, "_Particles", particleBuffer.Read);
    compute.SetBuffer(kernel, "_ParticlePoolConsume", poolBuffer);
    compute.SetBuffer(kernel, "_DividablePoolConsume", dividablePoolBuffer);
    compute.SetInt("_DivideCount", maxDivideCount);

    Dispatch1D(kernel, count);
}
//}

GetDividableParticlesカーネルによって、
dividablePoolBufferに分裂可能なParticle(activeになっているParticle)を追加し、
そのバッファを元に、実際に分裂処理を行うDivideParticlesカーネルを実行する回数をもとめます。

分裂処理の回数のもとめ方はDivideParticlesKernel関数の冒頭の通りで、

 * maxDivideCount
 * dividablePoolBufferが持つ分裂可能なParticle数、
 * poolBufferが持つオブジェクトプールに残っている非活性なParticle数

とを比較します。
これらの数値の比較によって、
分裂可能な数の制限を超えて分裂処理が走ることを防いでいます。

以下がカーネルの中身になります。

//emlist[CellularGrowth.compute]{
// 分裂できるParticleの候補を決定する関数
// ここの条件を変更することで分裂パターンを調整することができる
bool dividable_particle(Particle p, uint idx)
{
  // 成長率に応じて分裂
  float rate = (p.radius / p.threshold);
  return rate >= 0.95;

  // ランダムに分裂
  // return nrand(float2(idx, _Time)) < 0.1;
}

// Particleを分裂する関数
uint divide_particle(uint idx, float2 offset)
{
  Particle parent = _Particles[idx];
  Particle child = create();

  // サイズを半分に設定
  float rh = parent.radius * 0.5;
  rh = max(rh, 0.1);
  parent.radius = child.radius = rh;

  // 親と子の位置をずらす
  float2 center = parent.position;
  parent.position = center - offset;
  child.position = center + offset;

  // 子の最大サイズをランダムに設定
  float x = nrand(float2(_Time, idx));
  child.threshold = rh * lerp(1.25, 2.0, x);

  // 子のindexをオブジェクトプールから取得し、子Particleをバッファに設定
  uint cidx = _ParticlePoolConsume.Consume();
  _Particles[cidx] = child;

  // 親Particleを更新
  _Particles[idx] = parent;

  return cidx;
}

uint divide_particle(uint idx)
{
  Particle parent = _Particles[idx];

  // ランダムに位置をずらす
  float2 offset = 
    random_point_on_circle(float2(idx, _Time)) * 
    parent.radius * 0.25;

  return divide_particle(idx, offset);
}

...

THREAD
void GetDividableParticles(uint3 id : SV_DispatchThreadID)
{
  uint idx = id.x;
  uint count, strides;
  _Particles.GetDimensions(count, strides);
  if (idx >= count)
    return;

  Particle p = _Particles[idx];
  if (p.alive && dividable_particle(p, idx))
  {
    _DividablePoolAppend.Append(idx);
  }
}

THREAD
void DivideParticles(uint3 id : SV_DispatchThreadID)
{
  if (id.x >= _DivideCount)
    return;

  uint idx = _DividablePoolConsume.Consume();
  divide_particle(idx);
}
//}

これらの処理によって実現される細胞分裂の結果は以下のようになります。

//image[Nakamura/ParticleOnly][CellularGrowthParticleOnly.scene]

=== ネットワーク構造の表現 (CellularGrowth.cs)

細胞同士がくっつきあう様子を実現するため、Particle同士を結ぶEdgeを導入し、
細胞をネットワーク構造で表現します。

ここからはCellularGrowth.csの実装を通して解説を進めます。

EdgeはParticleが分裂するタイミングで追加され、
分裂したParticle同士をつなげます。

#@# 図

Edgeの構造は以下のように定義します。

//emlist[Edge.cs]{
[StructLayout(LayoutKind.Sequential)]
public struct Edge_t 
{
    public int a, b;        // Edgeが結ぶ2つのParticleのindex
    public Vector2 force;   // 2つのParticle同士をくっつけあわせる力
    uint alive;             // 活性化フラグ
}
//}

EdgeもParticleと同様に増減するため、Append/ConsumeStructuredBufferで管理します。

==== Division

ネットワーク構造の分裂は以下のような流れで行います。

 1. 分裂可能なEdgeの候補を取得し、dividablePoolBufferに格納
 2. 分裂可能なEdgeが空の場合は、接続Edges数が0のParticle(linksが0のParticle)を分裂させ、2つのParticleをEdgeで接続する
 3. 分裂可能なEdgeがある場合は、dividablePoolBufferからEdgeを取り出し分裂させる

実際に分裂するのはParticleなのですが、
ここで"分裂可能なEdge"と言っているのは、
後ほど紹介する分裂パターンにおいて、
分裂元のParticleと接続されているEdgeを処理する際に都合が良いため、
ネットワーク構造の分裂はEdge単位で行っています。

上に挙げた分裂の流れによって、
一つのParticleから分裂を繰り返し、
大きなネットワーク構造を生成することができます。

Edgeの分裂は前節のCellularGrowthParticleOnly.csと同様、コルーチンによって一定時間ごとに実行されます。

//emlist[CellularGrowth.cs]{
protected IEnumerator IDivider()
{
    yield return 0;
    while(true)
    {
        yield return new WaitForSeconds(divideInterval);
        Divide();
    }
}

protected void Divide()
{
    // 1. 分裂可能なEdgeの候補を取得し、dividablePoolBufferに格納
    GetDividableEdgesKernel();

    int dividableEdgesCount = CopyPoolSize(dividablePoolBuffer);
    if(dividableEdgesCount == 0)
    {
        // 2. 分裂可能なEdgeが空の場合は、
        // 接続Edges数が0のParticle(linksが0のParticle)を分裂させ、
        // 2つのParticleをEdgeで接続する
        DivideUnconnectedParticles();
    } else
    {
        // 3. 分裂可能なEdgeがある場合は、dividablePoolBufferからEdgeを取り出し分裂させる
        // 分裂パターン(後述)に応じてEdgeの分裂を実行する
        switch(pattern)
        {
            case DividePattern.Closed:
                // 閉じたネットワーク構造を生成するパターン
                DivideEdgesClosedKernel(
                    dividableEdgesCount, 
                    maxDivideCount
                );
                break;
            case DividePattern.Branch:
                // 枝分かれするパターン
                DivideEdgesBranchKernel(
                    dividableEdgesCount, 
                    maxDivideCount
                );
                break;
        }
    }
}

... 

protected void GetDividableEdgesKernel()
{
    // 分裂可能なEdgeを格納するバッファをリセット
    dividablePoolBuffer.SetCounterValue(0);

    var kernel = compute.FindKernel("GetDividableEdges");
    compute.SetBuffer(
        kernel, "_Particles", 
        particlePool.ObjectPingPong.Read
    );
    compute.SetBuffer(kernel, "_Edges", edgePool.ObjectBuffer);
    compute.SetBuffer(kernel, "_DividablePoolAppend", dividablePoolBuffer);

    // Particleの最大接続数
    compute.SetInt("_MaxLink", maxLink);

    Dispatch1D(kernel, count);
}

... 

protected void DivideUnconnectedParticles()
{
    var kernel = compute.FindKernel("DivideUnconnectedParticles");
    compute.SetBuffer(
        kernel, "_Particles", 
        particlePool.ObjectPingPong.Read
    );
    compute.SetBuffer(
        kernel, "_ParticlePoolConsume", 
        particlePool.PoolBuffer
    );
    compute.SetBuffer(kernel, "_Edges", edgePool.ObjectBuffer);
    compute.SetBuffer(kernel, "_EdgePoolConsume", edgePool.PoolBuffer);

    Dispatch1D(kernel, count);
}
//}

分裂可能なEdgeを取得するカーネル(GetDividableEdges)は以下の通りです。

//emlist[CellularGrowth.compute]{
// 分裂可能かどうかの判断を行う
bool dividable_edge(Edge e, uint idx)
{
  Particle pa = _Particles[e.a];
  Particle pb = _Particles[e.b];

  // Particleの接続数が最大接続数(_MaxLink)を超えず、
  // dividable_particleに定義された分裂条件を満たしていれば分裂可能とする
  return 
    !(pa.links >= _MaxLink && pb.links >= _MaxLink) && 
    (dividable_particle(pa, e.a) && dividable_particle(pb, e.b));
}

...

// 分裂可能なEdgeを取得する
THREAD
void GetDividableEdges(uint3 id : SV_DispatchThreadID)
{
  uint idx = id.x;
  uint count, strides;
  _Edges.GetDimensions(count, strides);
  if (idx >= count)
    return;

  Edge e = _Edges[idx];
  if (e.alive && dividable_edge(e, idx))
  {
    _DividablePoolAppend.Append(idx);
  }
}
//}

分裂可能なEdgeが存在しない場合は、
以下の接続しているEdgeのないParticleを分裂させるカーネル(DivideUnconnectedParticles)を実行します。

//emlist[CellularGrowth.compute]{
// indexがaのParticleとbのParticleをつなぐEdgeを生成する関数
void connect(int a, int b)
{
  // Edgeのオブジェクトプールから非活性なEdgeのindexを取り出す
  uint eidx = _EdgePoolConsume.Consume();

  // Atomic演算(後述)を用いて
  // 各Particleの接続数をインクリメントする
  InterlockedAdd(_Particles[a].links, 1);
  InterlockedAdd(_Particles[b].links, 1);

  Edge e;
  e.a = a;
  e.b = b;
  e.force = float2(0, 0);
  e.alive = true;
  _Edges[eidx] = e;
}

...

// 接続しているEdgeが存在しないParticleを分裂させる
THREAD
void DivideUnconnectedParticles(uint3 id : SV_DispatchThreadID)
{
  uint count, stride;
  _Particles.GetDimensions(count, stride);
  if (id.x >= count)
    return;

  uint idx = id.x;
  Particle parent = _Particles[idx];
  if (!parent.alive || parent.links > 0)
    return;

  // 親Particleから分裂した子Particleを生成
  uint cidx = divide_particle(idx);

  // 親Particleと子ParticleをEdgeで接続する
  connect(idx, cidx);
}
//}

分裂したParticle同士を接続するEdgeを生成するconnect関数では、
Atomic演算というテクニックを用いてParticleの接続数をインクリメントしています。

===[column] Atomic演算 (InterlockedAdd関数について)

あるスレッドがグローバルメモリやシェアードメモリ上のデータを読み込み、修正し、書き込むという一連の処理を行うとき、
その処理中にそのメモリ領域に他のスレッドからの書き込みが行われるなどして、値が変化するのを防ぎたい場合があります。
(並列処理特有の、スレッドがメモリにアクセスする順番によって結果が変化してしまうデータレース(データの競合)と呼ばれる現象)

これを保証するのがAtomic演算で、リソースの演算操作(四則演算や比較)中に他のスレッドからの干渉を防ぎ、
GPU上で安全に逐次的な処理を実現できます。

HLSLではこれらの操作を行う関数@<fn>{atomicfunctions}はInterlockedというprefixがついており、
本章の例ではInterlockedAddを用いています。

InterlockedAdd関数は、
第一引数に指定されたリソースに第二引数に指定された整数を足し合わせる処理で、
_Particles[index].linksに1を足すことで接続数をインクリメントしています。

こうすることでスレッド間で一貫性のある接続数の管理が実現でき、
矛盾なく接続数を増やしたり減らしたりすることが可能になります。

//footnote[atomicfunctions][https://docs.microsoft.com/ja-jp/windows/desktop/direct3d11/direct3d-11-advanced-stages-cs-atomic-functions]

===[/column]

分裂可能なEdgeがある場合はdividablePoolBufferからEdgeを取り出し分裂させます。
DividePatternというenumパラメータを用意していることからわかるように、
分裂には様々なパターンを適用することができます。

ここでは閉じたネットワーク構造を生成する分裂パターン(DividePattern.Closed)を紹介します。

===== 閉じたネットワーク構造(DividePattern.Closed)

閉じたネットワーク構造を生成するパターンでは、
以下の図のような分裂を行います。

//image[Nakamura/ClosedLink][閉じたネットワーク構造を生成するパターン(DividePattern.Closed)]

//emlist[CellularGrowth.cs]{
protected void DivideEdgesClosedKernel(
    int dividableEdgesCount, 
    int maxDivideCount = 16
)
{
    // 閉じたネットワーク構造に分裂するパターン
    var kernel = compute.FindKernel("DivideEdgesClosed");
    DivideEdgesKernel(kernel, dividableEdgesCount, maxDivideCount);
}

// 分裂パターンで共通の処理
protected void DivideEdgesKernel(
    int kernel, 
    int dividableEdgesCount, 
    int maxDivideCount
)
{
    // オブジェクトプールが空の状態でConsumeが呼ばれないように
    // maxDivideCountと各オブジェクトプールのサイズを比較
    maxDivideCount = Mathf.Min(dividableEdgesCount, maxDivideCount);
    maxDivideCount = Mathf.Min(particlePool.CopyPoolSize(), maxDivideCount);
    maxDivideCount = Mathf.Min(edgePool.CopyPoolSize(), maxDivideCount);
    if (maxDivideCount <= 0) return;

    compute.SetBuffer(
        kernel, "_Particles", 
        particlePool.ObjectPingPong.Read
    );
    compute.SetBuffer(
        kernel, "_ParticlePoolConsume", 
        particlePool.PoolBuffer
    );
    compute.SetBuffer(kernel, "_Edges", edgePool.ObjectBuffer);
    compute.SetBuffer(kernel, "_EdgePoolConsume", edgePool.PoolBuffer);

    compute.SetBuffer(kernel, "_DividablePoolConsume", dividablePoolBuffer);
    compute.SetInt("_DivideCount", maxDivideCount);

    Dispatch1D(kernel, maxDivideCount);
}
//}

閉じたネットワーク構造を生成するGPUカーネル(DivideEdgesClosed)で用いている関数divide_edge_closedは、
Particleが持つEdgeの数に応じて処理を変えます。

いずれか一方のParticleの接続数が1の場合、
分裂したParticleと足し合わせた3つのParticleで3角形を描くようにEdgeで繋ぎます。(@<img>{Nakamura/FigureClosedLink1})

#@# 図
//image[Nakamura/FigureClosedLink1][2つのParticleと分裂したParticleで3角形を描くように閉じたネットワークを形成する]

それ以外のケースでは、
分裂したParticleを既存の2つのParticleの間に挿入するようにEdgeを繋ぎ、
分裂元のParticleと繋がっていたEdgeを変換して閉じたネットワークが維持されるように処理します。
(@<img>{Nakamura/FigureClosedLink2})

#@# 図
//image[Nakamura/FigureClosedLink2][分裂したParticleを既存の2つのParticleの間に挿入し、閉じたネットワークが維持されるようにEdgeの接続関係を調整する]

こうした分裂処理を繰り返すことによって、
閉じたネットワーク構造が成長していきます。

//emlist[CellularGrowth.compute]{

// 閉じたネットワーク構造への分裂を実行する関数
void divide_edge_closed(uint idx)
{
  Edge e = _Edges[idx];

  Particle pa = _Particles[e.a];
  Particle pb = _Particles[e.b];

  if ((pa.links == 1) || (pb.links == 1))
  {
    // 3つのParticleで三角形を描くように分裂し、Edgeで繋ぐ
    uint cidx = divide_particle(e.a);
    connect(e.a, cidx);
    connect(cidx, e.b);
  }
  else 
  {
    // 2つのParticleの間にParticleを生成し、
    // 一繋ぎになるようにEdgeを繋ぐ
    float2 dir = pb.position - pa.position;
    float2 offset = normalize(dir) * pa.radius * 0.25;
    uint cidx = divide_particle(e.a, offset);

    // 親Particleと分裂した子Particleを結ぶ
    connect(e.a, cidx);

    // 元の2つのParticleを結んでいたEdgeを、
    // 分裂した子Particleを結ぶEdgeに変換する
    InterlockedAdd(_Particles[e.a].links, -1);
    InterlockedAdd(_Particles[cidx].links, 1);
    e.a = cidx;
  }

  _Edges[idx] = e;
}

...

// 閉じたネットワーク構造に分裂するパターン
THREAD
void DivideEdgesClosed(uint3 id : SV_DispatchThreadID)
{
  if (id.x >= _DivideCount)
    return;

  // 分裂可能なEdgeのindexを取得
  uint idx = _DividablePoolConsume.Consume();
  divide_edge_closed(idx);
}
//}

==== Edgeの引き合い

自然に存在する多くの細胞は他の細胞とくっつきあう性質を持ちます。
こうした性質を模倣するため、
Edgeは繋がった2つのParticleをバネのように引き合います。

 1. Edgeごとに2つのParticleを引き付けるバネの力を計算
 2. Particleごとに接続したEdgeが持つ力を加える

という流れでバネの引き合いを実現しています。

//emlist[CellularGrowth.cs]{
protected void Update() {
    ... 
    UpdateEdgesKernel();
    SpringEdgesKernel();
    ... 
}

... 

protected void UpdateEdgesKernel()
{
    // Edgeごとにバネが引き合う力を計算する
    var kernel = compute.FindKernel("UpdateEdges");
    compute.SetBuffer(
        kernel, "_Particles", 
        particlePool.ObjectPingPong.Read
    );
    compute.SetBuffer(kernel, "_Edges", edgePool.ObjectBuffer);
    compute.SetFloat("_Spring", spring);

    Dispatch1D(kernel, count);
}

protected void SpringEdgesKernel()
{
    // ParticleごとにEdgeが持つバネの力を加える
    var kernel = compute.FindKernel("SpringEdges");
    compute.SetBuffer(
        kernel, "_Particles", 
        particlePool.ObjectPingPong.Read
    );
    compute.SetBuffer(kernel, "_Edges", edgePool.ObjectBuffer);

    Dispatch1D(kernel, count);
}
//}

以下がカーネルの中身になります。

//emlist[CellularGrowth.compute]{
THREAD
void UpdateEdges(uint3 id : SV_DispatchThreadID)
{
  uint idx = id.x;
  uint count, strides;
  _Edges.GetDimensions(count, strides);
  if (idx >= count)
    return;

  Edge e = _Edges[idx];

  // 引き合う力を初期化
  e.force = float2(0, 0);

  if (!e.alive)
  {
    _Edges[idx] = e;
    return;
  }

  Particle pa = _Particles[e.a];
  Particle pb = _Particles[e.b];
  if (!pa.alive || !pb.alive)
  {
    _Edges[idx] = e;
    return;
  }

  // 2つのParticle間の距離を測り、
  // 離れていたり、近づきすぎていれば引き合う力を加える
  float2 dir = pa.position - pb.position;
  float r = pa.radius + pb.radius;
  float len = length(dir);
  if (abs(len - r) > 0)
  {
    // 適切な距離(互いの半径の合計)になるように力を加える
    float l = ((len - r) / r);
    float2 f = normalize(dir) * l * _Spring;
    e.force = f;
  }

  _Edges[idx] = e;
}

THREAD
void SpringEdges(uint3 id : SV_DispatchThreadID)
{
  uint idx = id.x;
  uint count, strides;
  _Particles.GetDimensions(count, strides);
  if (idx >= count)
    return;

  Particle p = _Particles[idx];
  if (!p.alive || p.links <= 0)
    return;

  // 接続数が多いほど、引き合う力を弱める
  float dif = 1.0 / p.links;

  int iidx = (int)idx;

  _Edges.GetDimensions(count, strides);

  // すべてのEdgeから自身と接続しているParticleを探す
  for (uint i = 0; i < count; i++)
  {
    Edge e = _Edges[i];
    if (!e.alive)
      continue;

    // 接続しているEdgeが見つかったら力を加える
    if (e.a == iidx)
    {
      p.velocity -= e.force * dif;
    }
    else if (e.b == iidx)
    {
      p.velocity += e.force * dif;
    }
  }

  _Particles[idx] = p;
}
//}

以上までの処理でネットワークで構成された細胞が成長していく様子を表現することができます。

=== 分割パターンのバリエーション

分割させるEdgeの判定(dividable_edge関数)と分割ロジックを調整することで、
様々な分割パターンをデザインすることができます。

サンプルプロジェクトのCellularGrowth.csでは、
分裂パターンをenumパラメータによって切り替えられるようにしています。

==== 枝分かれする分裂パターン (DividePattern.Branch)

枝分かれするパターンでは、
以下の@<img>{Nakamura/FigureBranchLink}のような分裂を行います。

分裂した子Particleは親Particleとのみ接続します。
これを繰り返すだけで枝分かれしたネットワークが成長します。

#@# 図
//image[Nakamura/FigureBranchLink][枝分かれする分裂パターン]

//emlist[CellularGrowth.cs]{
protected void DivideEdgesBranchKernel(
    int dividableEdgesCount, 
    int maxDivideCount = 16
)
{
    // 枝分かれする分裂パターンを実行
    var kernel = compute.FindKernel("DivideEdgesBranch");
    DivideEdgesKernel(kernel, dividableEdgesCount, maxDivideCount);
}
//}

//emlist[CellularGrowth.compute]{
// 枝分かれ分裂を実行する関数
void divide_edge_branch(uint idx)
{
  Edge e = _Edges[idx];
  Particle pa = _Particles[e.a];
  Particle pb = _Particles[e.b];

  // 接続数の少ない方のParticleindexを取得
  uint i = lerp(e.b, e.a, step(pa.links, pb.links));

  uint cidx = divide_particle(i);
  connect(i, cidx);
}

...

// 枝分かれ分裂パターン
THREAD
void DivideEdgesBranch(uint3 id : SV_DispatchThreadID)
{
  if (id.x >= _DivideCount)
    return;

  // 分裂可能なEdgeのindexを取得
  uint idx = _DividablePoolConsume.Consume();
  divide_edge_branch(idx);
}
//}

枝分かれするパターンにおいては、
分裂するEdgeを判定するロジックがビジュアルに大きく影響します。
dividable_edge関数内で参照しているParticleの最大接続数(_MaxLink)の値を変化させることで、
枝分かれ具合をコントロールすることができます。

//image[Nakamura/BranchLink2][_MaxLinkに2を設定したパターン(DividePattern.Branch)]

//image[Nakamura/BranchLink3][_MaxLinkに3を設定したパターン(DividePattern.Branch)]

//image[Nakamura/BranchLinkChanged][_MaxLinkを3に設定してある程度成長させた後、2に設定して成長を続けさせたパターン(DividePattern.Branch)]

== まとめ

本章では、GPU上で細胞の分裂と成長をシミュレーションするプログラムを紹介しました。

こうした細胞をモチーフとしたCGを生成する試みは他にも、
Andy Lomas@<fn>{andylomas}によるMorphogenetic Creationsプロジェクトや、
学術的なものだとJ.A.Kaandorp@<fn>{jakaandorp}によるComputational Biologyプロジェクトがあり、
特に後者のものは生物学に基づいたよりリアルなシミュレーションを行っています。

また、Maxime Causeret@<fn>{maximecauseret}によるMax Cooperのミュージックビデオ@<fn>{maxcooper}が
細胞などの有機的なモチーフを使用した素晴らしい映像作品の例として挙げられます。
(この映像作品内のシミュレーション部分にはHoudiniが使われています)

今回は2次元上に分裂・成長するものに留まりましたが、
元のiGeoのチュートリアル@<fn>{tutorial56}にもあるように、
本プログラムは3次元上に拡張することも可能です。

3次元への拡張では、
3つの細胞から面を構成し、成長して広がる細胞ネットワークを用いて、
グニグニと有機的に成長するメッシュを実現することもできます。
3次元への拡張を行っているサンプルは https://github.com/mattatz/CellularGrowth に上げているので、
興味のある方は参考にしてみてください。

//footnote[andylomas][http://www.andylomas.com/]
//footnote[jakaandorp][https://staff.fnwi.uva.nl/j.a.kaandorp/research.html]
//footnote[maximecauseret][http://teresuac.fr/]
//footnote[maxcooper][https://vimeo.com/196269431]
//footnote[houdini][https://www.sidefx.com/]
//footnote[tutorial56][http://igeo.jp/tutorial/56.html]

== 参考

 * http://igeo.jp/tutorial/55.html
 * https://msdn.microsoft.com/ja-jp/library/ee422322(v=vs.85).aspx
