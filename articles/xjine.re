= 柔らかな変形を簡単に表現する

//image[xjine/01][球体が変形する様子]

オブジェクトの柔らかさを表現するとき、バネを模したり、流体や軟体のシミュレーションを計算したりすることがありますが、
ここではそれほど大げさな計算をするでもなく、オブジェクトの柔らかな変形を表現してみます。図にあるように手描きアニメのような変形です。

本章のサンプルは@<br>{}
@<href>{https://github.com/IndieVisualLab/UnityGraphicsProgramming3}@<br>{}
の「OverReaction」です。

== サンプルシーンの動かし方

「OverReaction」のシーンでは、基本的な変形を確認することができます。
マニピュレータや Inspector からオブジェクトを動かして変形するのを確認してください。

「PhysicsScene」のシーンでは、オブジェクトに対して上下左右のキーで力を加えることができます。
シーン上にいくつかのオブジェクトを置くと、状況に応じて変形する様子を確認することができます。

== 運動エネルギーの算出

変形の規則はいくつも考えられますが、基本的なルールは 3 つ程度でしょう。

 1. 変化が大きいほど大きく変形する。
 2. 変化がないときは変形が徐々に戻る。
 3. 変化の方向が反転するときは変形の方向も反転する。

ここでは特にオブジェクトが動くときを考慮しますから、まずは動きの方向とその大きさを検出していきます。
物理法則で用いられる用語と異なりますが、便宜上このパラメータを「運動エネルギー」@<code>{moveEnergy} と呼びます。
運動エネルギーは方向と大きさで表されるパラメータですから、ベクトルで表現することができます。

※正しい物理学用語としての運動エネルギーは「kinetic energy」です。
ここでは特に移動のみを考慮したエネルギーのため「move energy」で命名しています。

ゲームプログラミングにおいては当たり前のように扱われるので今更ですが、オブジェクトの動きは単純に座標の変化を検出します。
一点だけ注意したいのは、@<code>{Update} ではなく @<code>{FixedUpdate} を採用している点です。

//emlist[OverReaction.cs]{
protected void FixedUpdate()
{
    this.crntMove = this.transform.position - this.prevPosition;

    UpdateMoveEnergy();
    UpdateDeformEnergy();
    DeformMesh();

    this.prevPosition = this.transform.position;
    this.prevMove = this.crntMove;
}
//}

@<code>{FixedUpdate} は一定時間ごとに呼び出されるメソッドですから、
秒間当たりに 2 度 3 度 ~ と呼び出される可能性のある @<code>{Update} とは明らかに性質が異なります。
これらの違いについて詳細を解説すると本題から外れるので割愛しますが、
ここでは Unity の PhysX(物理挙動) を使ったオブジェクトの動きにも対応したいために @<code>{FixedUpdate} を採用しました。
また @<code>{Update} のような頻度でメッシュを変形する必要性も特にないです。

ではオブジェクトの動きが座標の変化から算出できたところで、これから運動エネルギーを算出しましょう。
運動エネルギーの算出は @<code>{UpdateMoveEnergy} メソッドに実装されています。

//emlist[OverReaction.cs]{
protected void UpdateMoveEnergy()
{
    this.moveEnergy = new Vector3()
    {
        x = UpdateMoveEnergy
        (this.crntMove.x, this.prevMove.x, this.moveEnergy.x),

        y = UpdateMoveEnergy
        (this.crntMove.y, this.prevMove.y, this.moveEnergy.y),

        z = UpdateMoveEnergy
        (this.crntMove.z, this.prevMove.z, this.moveEnergy.z),
    };
}
//}

運動エネルギーは X, Y, Z 方向のそれぞれの成分に分解して算出します。
以降に @<code>{UpdateMoveEnergy} の処理を順に追って解説していきます。

まずは現在の動きがない場合を考えます。動きがないとき、現存する運動エネルギーは減衰していきます。

//emlist[OverReaction.cs]{
protected float UpdateMoveEnergy
(float crntMove, float prevMove, float moveEnergy)
{
    int crntMoveSign = Sign(crntMove);
    int prevMoveSign = Sign(prevMove);
    int moveEnergySign = Sign(moveEnergy);

    if (crntMoveSign == 0)
    {
        return moveEnergy * this.undeformPower;
    }
…
}

public static int Sign(float value)
{
    return value == 0 ? 0 : (value > 0 ? 1 : -1);
}
//}

現在の動きと直前の動きとが反転しているときは、運動エネルギーを反転します。

//emlist[OverReaction.cs]{
if (crntMoveSign != prevMoveSign)
{
    return moveEnergy - crntMove;
}
//}

現在の動きと運動エネルギーとが反転しているときは、運動エネルギーを小さくします。

//emlist[OverReaction.cs]{
if (crntMoveSign != moveEnergySign)
{
    return moveEnergy + crntMove;
}
//}

上記以外のケース、現在の動きと運動エネルギーが同じ方向のときは、
現在の動きと現存する運動エネルギーとを比較して、大きい方を採用します。

ただし運動エネルギーは減衰して小さくなっていきます。
また、現在の動きが発生させる新たな運動エネルギーは、変形を演出しやすいように、任意のパラメータを乗算して増強します。

//emlist[OverReaction.cs]{
if (crntMoveSign < 0)
{
    return Mathf.Min(crntMove * this.deformPower,
                     moveEnergy * this.undeformPower);
}
else
{
    return Mathf.Max(crntMove * this.deformPower,
                     moveEnergy * this.undeformPower);
}
//}

これで変形に利用するための運動エネルギーは算出することができました。

== 変形エネルギーの算出

次に、算出された運動エネルギーを変形を決定するパラメータに変換します。
便宜上、このパラメータを「変形エネルギー」@<code>{deformEnergy} と呼びます。
変形エネルギーは @<code>{UpdateDeformEnergy} メソッドで更新されます。

変形エネルギーの大きさは、そのまま運動エネルギーの大きさと定義することもできますが、
変形エネルギーの方向と、オブジェクトが動いている方向とにズレが生じている場合、変形エネルギーは完全にはオブジェクトに伝わりません。
また変形エネルギーの方向と、オブジェクトが動いている方向とが反転してしまっているケースも考えられます。

そこで変形エネルギーと現在の動きとの内積から、変形エネルギーがどの程度伝わるのかを求めます。
もし完全に方向が一致していれば単位ベクトル同士の内積は 1 となり、ズレの大きさによって徐々に 0 に近づいていきます。
さらに反転するときは負の値となります。

//emlist[OverReaction.cs]{
protected void UpdateDeformEnergy()
{
    float deformEnergyVertical
    = this.moveEnergy.magnitude
    * Vector3.Dot(this.moveEnergy.normalized,
                  this.crntMove.normalized);
    …
//}

オブジェクトを垂直方向に変形する力が算出できたので、垂直方向へ変化した分だけ、水平方向を変形します。
つまり垂直方向にオブジェクトが伸びたなら水平方向に縮ませようということです。
逆に、垂直方向に縮むときは水平方向に伸びるようにします。

垂直方向へどれだけ変形したのかは「垂直方向への変形の大きさ / 最大の変形の大きさ」で算出されます。
あとは垂直方向へ変形したのと同じだけ、水平方向にも変形するように算出します。

仮に垂直方向に +0.8 変形したとして、水平方向へは -0.8 変形すればよいので、
水平方向の変形エネルギーは 1 - 0.8 = 0.2 となります。
また、実際に変形するときの係数として * 0.8 では小さくなりますから、1 を足して * 1.8 となるようにします。

//emlist[OverReaction.cs]{
protected void UpdateDeformEnergy()
{
…
    float deformEnergyHorizontalRatio
    = deformEnergyVertical / this.maxDeformScale;

    float deformEnergyHorizontal
    = 1 - deformEnergyHorizontalRatio;
…
    deformEnergyVertical = 1 + deformEnergyVertical;
}
//}

最後にオブジェクトが進行方向に潰れるケースも考慮しましょう。
オブジェクトが進行方向に潰れるケースとは、運動エネルギーと現在の動きが反転しているときで、
つまり先の「運動エネルギーと現在の動きの内積」が負のときです。

内積の値が負のとき、垂直方向への変形エネルギーと、水平方向への変形エネルギーを逆転します。

場合分けを考えながら理解するために、次のコードはこれまでの手順を一続きにしました。
内積の値が負のとき、@<code>{deformEnergyHorizontal} は 1 より大きい正の値になります。
また @<code>{deformEnergyVertical} は、@<code>{deformEnergyHorizontal} の値と逆転して、1 より小さい正の値になります。

//emlist[OverReaction.cs]{
protected void UpdateDeformEnergy()
{
    float deformEnergyVertical
    = this.moveEnergy.magnitude
    * Vector3.Dot(this.moveEnergy.normalized,
                  this.crntMove.normalized);

    float deformEnergyHorizontalRatio
    = deformEnergyVertical / this.maxDeformScale;

    float deformEnergyHorizontal
    = 1 - deformEnergyHorizontalRatio;

    if (deformEnergyVertical < 0)
    {
        deformEnergyVertical = deformEnergyHorizontalRatio;
    }

    deformEnergyVertical = 1 + deformEnergyVertical;
…
//}

最後に変形エネルギーが任意に設定される範囲に収まるように値を修正して、変形エネルギーの算出を完了します。

//emlist[OverReaction.cs]{
deformEnergyVertical = Mathf.Clamp(deformEnergyVertical,
                                    this.minDeformScale,
                                    this.maxDeformScale);

deformEnergyHorizontal = Mathf.Clamp(deformEnergyHorizontal,
                                        this.minDeformScale,
                                        this.maxDeformScale);

this.deformEnergy = new Vector3(deformEnergyHorizontal,
                                deformEnergyVertical,
                                deformEnergyHorizontal);
//}

== メッシュを変形する

ここでは説明と汎用化のためにスクリプトでメッシュを変形しています。メッシュの変形は @<code>{DeformMesh} メソッドに実装されます。

※行列演算ですから、実用的にはシェーダを使って GPU で処理する方が良いケースが多いでしょう。

得られた変形エネルギー @<code>{deformEnergy} は、運動エネルギー @<code>{moveEnergy} の方向を向くときの伸縮を表すベクトルです。
したがって、変形するときは座標を合わせてから変形する必要があります。
そのために必要となるパラメータを先に抑えておきます。
現在のオブジェクトの回転行列とその逆行列、運動エネルギー @<code>{moveEnergy} の回転行列とその逆行列です。

//emlist[OverReaction.cs]{
protected void DeformMesh()
{
Vector3[] deformedVertices = new Vector3[this.baseVertices.Length];

Quaternion crntRotation  = this.transform.localRotation;
Quaternion crntRotationI = Quaternion.Inverse(crntRotation);

Quaternion moveEnergyRotation
= Quaternion.FromToRotation(Vector3.up, this.moveEnergy.normalized);
Quaternion moveEnergyRotationI = Quaternion.Inverse(moveEnergyRotation);
…
//}

 1. 現在の回転行列を、回転していないメッシュの頂点に乗算して回転します。
 2. 移動方向を示す回転行列の逆行列を、頂点に乗算して回転します。
 3. 頂点を @<code>{deformEnergy} にしたがってスケーリングします。
 4. 移動方向を示す回転行列を、頂点に乗算して回転を元に戻します。
 5. 現在の回転行列の逆行列を、頂点に乗算して回転を元に戻します。

分かりやすく適当な変形エネルギー @<code>{deformEnergy} を仮に与え、
順次ソースコードをコメントアウトすると、処理手順が分かりやすくなると思います。

//emlist[OverReaction.cs]{
for (int i = 0; i < this.baseVertices.Length; i++)
{
    deformedVertices[i] = this.baseVertices[i];
    deformedVertices[i] = crntRotation * deformedVertices[i];
    deformedVertices[i] = moveEnergyRotationI * deformedVertices[i];
    deformedVertices[i] = new Vector3(
        deformedVertices[i].x * this.deformEnergy.x,
        deformedVertices[i].y * this.deformEnergy.y,
        deformedVertices[i].z * this.deformEnergy.z);
    deformedVertices[i] = moveEnergyRotation * deformedVertices[i];
    deformedVertices[i] = crntRotationI * deformedVertices[i];
}

this.baseMesh.vertices = deformedVertices;
//}

== まとめ

非常に簡単な実装だけでオブジェクトを変形することができました。これだけの実装な手軽な反面、見た目に与える印象は大きく変わります。

算出コストはさらに要しますが、発展形として、オブジェクトの回転や拡大縮小にも対応したり、
変形の重心を移動したり、スキンメッシュアニメーションに対応することも考えられます。