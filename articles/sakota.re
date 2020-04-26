= Strange Attractor

== はじめに

本章では「Strange Attractor」と呼ばれる、状態が特定の微分方程式や差分方程式によって、非線形のカオス的な振る舞いを見せる現象の可視化をUnityとGPU演算を使って開発していきます。@<br>{}
本章のサンプルは@<br>{}
@<href>{https://github.com/IndieVisualLab/UnityGraphicsProgramming3}@<br>{}
の「StrangeAttractors」です。

=== 実行環境
 * ComputeShaderが実行できる、シェーダーモデル5.0対応環境
 * 執筆時環境、Unity2018.2.9f1で動作確認済み

== Strange Attractorとは
散逸系（エネルギー非保存。特定の入力と開放がある非均衡系）の運動でありながら、時間経過に伴って安定した軌道を保つ状態を「Attractor」と言います。@<br>{}
その中でも初期状態の僅かな差が時間経過にしたがって増幅され、カオス的な振る舞いを見せるものを「Strange Attractor」と呼びます。

本章ではその中から題材として、「@<fn>{lorenz}Lorenz Attractor」と「@<fn>{thomas}Thomas' Cyclically Symmetric Attractor」を取り上げていきたいと思います。

== Lorenz Attractor
バタフライ効果という現象をご存知でしょうか。これは気象学者のEdward N Lorenzが1972年にアメリカ科学振興協会で行った講演のタイトル
「@<fn>{butterflyEffect}ブラジルの1匹の蝶の羽ばたきはテキサスで竜巻を引き起こすか？」に由来する言葉です。@<br>{}
この言葉は、初期値の僅かな差が数学的に必ずしも似た結果をもたらさず、カオス的に増幅され予測困難な振る舞いを見せる現象をあらわしています。

この数学的性質を指摘したLorenzが1963年に発表したのが「Lorenz Attractor」です。

//image[sakota/LorenzAttractor1][Lorenz attractorの初期状態][scale=1.0]{
//}
//image[sakota/LorenzAttractor2][Lorenz attractorの中期][scale=1.0]{
//}

//footnote[lorenz][Lorenz, E. N.： Deterministic Nonperiodic Flow, Journal of Atmospheric Sciences, Vol.20, pp.130-141, 1963.]
//footnote[thomas][Thomas, René（1999）. "Deterministic chaos seen in terms of feedback circuits: Analysis, synthesis, 'labyrinth chaos'". Int. J. Bifurcation and Chaos. 9 （10）: 1889–1905.]
//footnote[butterflyEffect][http://eaps4.mit.edu/research/Lorenz/Butterfly_1972.pdf]

=== Lorenz方程式
Lorenz方程式は次の非線形常微分方程式で表されます。

//embed[latex]{
  \begin{align*}
  & \frac{\mathrm{d} x }{\mathrm{d} t} = -px + py \\
  & \frac{\mathrm{d} y }{\mathrm{d} t} = -xz + rx - y \\
  & \frac{\mathrm{d} z }{\mathrm{d} t} = xy - bz \\
  \end{align*}
//}


上記の方程式のp、r、bの各変数において、p=10、r=28、b=8/3と定めることにより、「Strange Attractor」としてカオス的に振る舞う状態になります。

=== Lorenz Attractorの実装
それではLorenz方程式をコンピュートシェーダーによって実装してみましょう。まずはコンピュートシェーダー内で演算したい構造体を定義します。@<br>{}

//emlist[StrangeAttractor.cs]{
protected struct Params
{
    Vector3 emitPos;
    Vector3 position;
    Vector3 velocity; // xyz = velocity, w = velocity coef;
    float   life;
    Vector2 size;     // x = current size, y = target size.
    Vector4 color;

    public Params(Vector3 emitPos, float size, Color color)
    {
        this.emitPos = emitPos;
        this.position = Vector3.zero;
        this.velocity = Vector3.zero;
        this.life = 0;
        this.size = new Vector2(0, size);
        this.color = color;
    }
}
//}

なお、この構造体は今後複数のStrange Attractorで汎用的に使う予定ですので、抽象クラスのStrangeAttractor.cs内に定義しています。@<br>{}

次にComputeBufferの初期化処理を行います。@<br>{}

//emlist[LorenzAttrator.cs]{
protected sealed override void InitializeComputeBuffer()
{
    if (cBuffer != null)　cBuffer.Release();

    cBuffer = new ComputeBuffer(instanceCount, Marshal.SizeOf(typeof(Params)));
    Params[] parameters = new Params[cBuffer.count];
    for (int i = 0; i < instanceCount; i++)
    {
        var normalize = (float)i / instanceCount;
        var color = gradient.Evaluate(normalize);
        parameters[i] = new Params(Random.insideUnitSphere *
            emitterSize * normalize, particleSize, color);
    }
    cBuffer.SetData(parameters);
}
//}

抽象クラスのStrangeAttractor.csで定義した抽象メソッドInitializeComputeBufferをLorenzAttrator.csにて実装しています。@<br>{}
Unityのインスペクターにてグラデーションやエミッターサイズ、粒子サイズを調整したいので、
インスペクターに露出したgradientやemitterSize、particleSizeでParams構造体を初期化し、ComputeBufferの変数、cBufferにSetDataします。@<br>{}
今回はパーティクルのid順で、少しずつ遅延させて速度ベクトルを適用していきたいと思っていますので、パーティクルid順でグラーデーションカラーをつけています。@<br>{}
「Strange Attractor」はものによって初期ポジションがその後の振る舞いに大きく関係してきますので、色々な初期ポジションを試していただきたいのですが、本サンプルでは球体を初期形状とします。@<br>{}

次に、LorenzAttratorの変数p、r、bをコンピュートシェーダーに渡します。@<br>{}

//emlist[LorenzAttrator.cs]{
[SerializeField, Tooltip("Default is 10")]
float p = 10f;
[SerializeField, Tooltip("Default is 28")]
float r = 28f;
[SerializeField, Tooltip("Default is 8/3")]
float b = 2.666667f;

private int pId, rId, bId;
private string pProp = "p", rProp = "r", bProp = "b";

protected override void InitializeShaderUniforms()
{
    pId = Shader.PropertyToID(pProp);
    rId = Shader.PropertyToID(rProp);
    bId = Shader.PropertyToID(bProp);
}

protected override void UpdateShaderUniforms()
{
    computeShaderInstance.SetFloat(pId, p);
    computeShaderInstance.SetFloat(rId, r);
    computeShaderInstance.SetFloat(bId, b);
}
//}

次にコンピュートシェーダー側でエミット時のパーティクルの状態を初期化します。@<br>{}

//emlist[LorenzAttractor.compute]{
#pragma kernel Emit
#pragma kernel Iterator

#define THREAD_X 128
#define THREAD_Y 1
#define THREAD_Z 1
#define DT 0.022

struct Params
{
    float3 emitPos;
    float3 position;
    float3 velocity; //xyz = velocity
    float  life;
    float2 size;     // x = current size, y = target size.
    float4 color;
};

RWStructuredBuffer<Params> buf;

[numthreads(THREAD_X, THREAD_Y, THREAD_Z)]
void Emit(uint id : SV_DispatchThreadID)
{
    Params p = buf[id];
    p.life = (float)id * -1e-05;
    p.position = p.emitPos;
    p.size.x = 0.0;
    buf[id] = p;
}
//}

Emitメソッドで初期化を行なっています。p.lifeはパーティクルの発生してからの時間を管理しており、初期値の時点でid毎に微量の遅延を設けています。@<br>{}
これはパーティクルが一斉に同じ軌道を描くのを簡易に防ぐためです。また、id毎のグラデーションカラーを設定していますので、カラーをきれいに見せるためにも役立ちます。@<br>{}
ここでは、パーティクルサイズのp.sizeを初期段階で0にしていますが、これは発生した瞬間のパーティクルを不可視にすることによって、自然な吹き出しにする為です。@<br>{}

次にイテレーション部分を見ていきます。@<br>{}

//emlist[LorenzAttractor.compute]{
#define DT 0.022

// Lorenz Attractor parameters
float p;
float r;
float b;

//Lorenz方程式の演算部分です。
float3 LorenzAttractor(float3 pos)
{
    float dxdt = (p * (pos.y - pos.x));
    float dydt = (pos.x * (r - pos.z) - pos.y);
    float dzdt = (pos.x * pos.y - b * pos.z);
    return float3(dxdt, dydt, dzdt) * DT;
}

[numthreads(THREAD_X, THREAD_Y, THREAD_Z)]
void Iterator(uint id : SV_DispatchThreadID)
{
    Params p = buf[id];
    p.life.x += DT;
    //速度ベクトルのベクトル長を0から1でクランプしつつ、サイズに掛ける事によってスタートを自然に見せています。
    p.size.x = p.size.y * saturate(length(p.velocity));
    if (p.life.x > 0)
    {
        p.velocity = LorenzAttractor(p.position);
        p.position += p.velocity;
    }
    buf[id] = p;
}
//}

上記のLorenzAttractorメソッドが「Lorenz方程式」の演算部分になります。微量なデルタタイムでのx,y,zの速度ベクトルを演算し、最後にデルタタイムを掛け、移動量を導き出しています。@<br>{}
経験上、コンピュートシェーダー内で形状に関わる微分演算を行う際は、Unityからのデルタタイムを送らずに、フレームレート差から独立した固定値のデルタタイムを使用した方が安定した形状を保ちます。@<br>{}
これはフレームレートが落ちすぎた場合に、UnityのTime.deltaTimeの数値が微分演算を行うにしては大きくなりすぎることがある為です。デルタ幅が大きくなれば、それだけ演算結果が前回の物と比べ
荒い大雑把な形状になってしまいます。@<br>{}
また、別の理由として、「Strange Attractor」はその方程式によっては、デルタタイムの取り方によって完全な収束もしくは無限大の発散をしてしまう場合がある為です。@<br>{}
これら2つの理由から、今回DTは定義済みの物を使用しています。


== Thomas' Cyclically Symmetric Attractor
次に生物学者のRené Thomas氏により発表された「Thomas' Cyclically Symmetric Attractor」の実装をしていきたいと思います。@<br>{}初期値に左右されず、時間経過にしたがって安定状態になり、形状も非常にユニークな物になっています。

//image[sakota/ThomasAttractor2][Thomas' Cyclically Symmetric Attractorの安定期][scale=1.0]{
//}

=== Thomas' Cyclically Symmetric 方程式
方程式は次の非線形常微分方程式で表されます。

//embed[latex]{
  \begin{eqnarray*}
  \frac{\mathrm{d} x }{\mathrm{d} t} = \sin y - bx \\
  \frac{\mathrm{d} y }{\mathrm{d} t} = \sin z - by \\
  \frac{\mathrm{d} z }{\mathrm{d} t} = \sin x - bz \\
  \end{eqnarray*}
//}

上記の方程式の変数bにおいて、@<m>{b \simeq 0.208186}として定めた場合、カオス的に「Strange Attractor」として振る舞い、@<m>{b \simeq 0}と定めた場合は空間を漂う形となります。

=== Thomas' Cyclically Symmetric Attractorの実装
それでは「Thomas' Cyclically Symmetric 方程式」をコンピュートシェーダーによって実装してみましょう。@<br>{}
前述の「Lorenz Attractor」の実装と共通する部分がある為、パラメーターの構造体や手続き的な部分は継承し必要な部分のみ取り上げます。@<br>{}
まずは、CPU側でカラーと初期ポジションをオーバーライドし定めます。@<br>{}

//emlist[ThomasAttractor.cs]{
protected sealed override void InitializeComputeBuffer()
{
    if (cBuffer != null)　cBuffer.Release();

    cBuffer = new ComputeBuffer(instanceCount, Marshal.SizeOf(typeof(Params)));
    Params[] parameters = new Params[cBuffer.count];
    for (int i = 0; i < instanceCount; i++)
    {
        var normalize = (float)i / instanceCount;
        var color = gradient.Evaluate(normalize);
        parameters[i] = new Params(Random.insideUnitSphere *
            emitterSize * normalize, particleSize, color);
    }
    cBuffer.SetData(parameters);
}
//}

今回は色をきれいに見せる為に、内側から外側にいくにつれてマントル的にグラデーションカラーをつけた球体を初期ポジションとして定めています。@<br>{}

次にエミット時とイテレーション時のコンピュートシェーダーのメソッドを見ていきます。@<br>{}

//emlist[ThomasAttractor.compute]{
//Thomas Attractor parameters
float b;

float3 ThomasAttractor(float3 pos)
{
    float dxdt = -b * pos.x + sin(pos.y);
    float dydt = -b * pos.y + sin(pos.z);
    float dzdt = -b * pos.z + sin(pos.x);
    return float3(dxdt, dydt, dzdt) * DT;
}

[numthreads(THREAD_X, THREAD_Y, THREAD_Z)]
void Emit(uint id : SV_DispatchThreadID)
{
    Params p = buf[id];
    p.life = (float)id * -1e-05;
    p.position = p.emitPos;
    p.size.x = p.size.y;
    buf[id] = p;
}

[numthreads(THREAD_X, THREAD_Y, THREAD_Z)]
void Iterator(uint id : SV_DispatchThreadID)
{
    Params p = buf[id];
    p.life.x += DT;
    if (p.life.x > 0)
    {
        p.velocity = ThomasAttractor(p.position);
        p.position += p.velocity;
    }
    buf[id] = p;
}
//}

ThomasAttractorメソッドが「Thomas' Cyclically Symmetric 方程式」の演算部分になります。@<br>{}
また、Emit時の実装は、LorenzAttratorと違い、今回は初期ポジションをあえて見せたいので、初期サイズからターゲットサイズに設定しています。@<br>{}
その他はほぼ同じ実装になります。

== まとめ

本章では、「Strange Attractor」をコンピュートシェーダーを用いてGPU実装する例をご紹介しました。@<br>{}
「Strange Attractor」にはさまざまな種類があり、実装においても比較的少ない演算でカオス的な振る舞いを見せる為、グラフィックスにおいても有用なアクセントになるのではないでしょうか。@<br>{}
他にも、「@<fn>{japanese}UedaAttractor」と呼ばれる2次元運動のものや、「@<fn>{aizawa}AizawaAttractor」のようなスピン運動を見せるもの等多種多様ありますので、もし興味がございましたらぜひ挑戦してみてください。

//footnote[japanese][http://www-lab23.kuee.kyoto-u.ac.jp/ueda/Kambe-Bishop_ver3-1.pdf]
//footnote[aizawa][http://www.algosome.com/articles/aizawa-attractor-chaos.html]

== 参考

 * http://paulbourke.net/fractals/lorenz/
 * https://en.wikipedia.org/wiki/Thomas%27_cyclically_symmetric_attractor
 * Lorenz, E. N.： Deterministic Nonperiodic Flow, Journal of Atmospheric Sciences, Vol.20, pp.130-141, 1963.
 * Thomas, René（1999）. "Deterministic chaos seen in terms of feedback circuits: Analysis, synthesis, 'labyrinth chaos'". Int. J. Bifurcation and Chaos. 9 （10）: 1889–1905.
