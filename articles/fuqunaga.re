
= PortalをUnityで実装してみた

== はじめに

Portal@<fn>{portal}というゲームをご存知でしょうか？
2007年にValve社から発売されパズルアクションゲームです。神ゲーです。
特徴はポータルと呼ばれる穴で、２箇所の穴がワームホームでつながっているかのように向こう側の景色を見ることができオブジェクトや自キャラがくぐってワープすることができます。
要はどこでもドアです。ポータルガンと呼ばれる銃で平面にポータルを設置することができ、これを駆使してゲームを進めていきます。
本章はこのポータルの機能を簡易的ながらUnityで実装してみた記事になります。
サンプルは@<br>{}
@<href>{https://github.com/IndieVisualLab/UnityGraphicsProgramming3}@<br>{}
の「PortalGateSystem」になります。

//footnote[portal][https://ja.wikipedia.org/wiki/Portal_(%E3%82%B2%E3%83%BC%E3%83%A0)]

== プロジェクトの概要

ポータルで遊ぶ場として必要な要素を考えます。

 * 移動できる自キャラ
 * フィールド
 * ポータルガン（指定位置にポータルを出現させる機能）

 あたりは欲しいです。
自キャラはポータルの向こう側に見えることがあるので一人称視点ですが全身のモデルが必要になります。
今回はUnity社から配布されている@<b>{Adam}@<fn>{adam}のモデルを使わせてもらいました。
また自キャラ以外にも物体がワープするさまを見たいのでEボタンで赤いボールを打ち出せるようにしています。

//footnote[adam][https://assetstore.unity.com/packages/essentials/tutorial-projects/adam-character-pack-adam-guard-lu-74842]


操作方法としては次のようにしました。

 * 移動： WASDキー、もしくは矢印キー（シフト押しながらでダッシュ）
 * 視点移動： マウス移動
 * ポータル発射： マウス左右クリック
 * ボール発射： Eキー
 * ジャンプ： スペースキー

なお以降ではワープする穴をゲートと呼んでいます。ソースコード上では@<b>{PortalGate}というクラス名になっています。

=== 自キャラ

自キャラはUnityのStandard Assets @<fn>{stdasset}を改造する形で作りました。
FirstPersonCharacterの制御を改造して使いつつ、ThirdPersonChracterのアニメーションを使用する感じです。
視界（メインカメラ）に自キャラ自身が写ってしまうとポリゴンがめり込んだり綺麗ではないので
@<b>{Player}レイヤーを設けてメインカメラでは@<b>{Player}レイヤーをカメラのCullingMaskに設定して映らないようにしています。

//footnote[stdasset][https://assetstore.unity.com/packages/essentials/asset-packs/standard-assets-32351]


=== フィールド

フィールドは@<b>{unity3d-jp}さんのレベルデザイン用アセット@<b>{playGROWnd}@<fn>{playg}を使わせてもらいました。
なんとなく雰囲気もPortalっぽいです。
今回はあとの処理を簡単にするためにフィールドは直方体の部屋としました。
コリジョンもオブジェクトのものはそのまま使わず直方体の各面ごとに透明なコリジョンを置いています。
ワープ後オブジェクトが部分的に部屋の外側に存在することもあるので落下防止用に床のコリジョンは部屋よりも広げています。
こちらは@<b>{StageColl}レイヤーにしています。

//footnote[playg][https://github.com/unity3d-jp/playgrownd]



== ゲートの生成

それではゲートを実装していきましょう。
今回のゲートはローカル座標系でXY平面上に広がりZ+方向を通過する向きとしています。

//image[fuqunaga/gate_coorinate][ゲートの座標系][scale=0.5]

発生はオリジナルPortalを踏襲してマウスをクリックすると視点先の平面にゲートが出現するようにし、
左クリックと右クリックでお互いがペアとして繋がります。
すでにゲートがある場合は古いゲートはその場で消滅し新しいゲートが開きます。
内部的には古いゲートを新しい場所に移動して最初期化しています。

//emlist[PortalGun.cs]{
void Shot(int idx)
{
    RaycastHit hit;
    if (Physics.Raycast(transform.position,
        transform.forward, 
        out hit,
        float.MaxValue,
        LayerMask.GetMask(new[] { "StageColl" })))
    {
        var gate = gatePair[idx];
        if (gate == null)
        {
            var go = Instantiate(gatePrefab);
            gate = gatePair[idx] = go.GetComponent<PortalGate>();

            var pair = gatePair[(idx + 1) % 2];
            if (pair != null)
            {
                gate.SetPair(pair);
                pair.SetPair(gate);
            }
        }

        gate.hitColl = hit.collider;

        var trans = gate.transform;
        var normal = hit.normal;
        var up = normal.y >= 0f ? transform.up : transform.forward;

        trans.position = hit.point + normal * gatePosOffset;
        trans.rotation = Quaternion.LookRotation(-normal, up);

        gate.Open();
    }
}
//}

@<b>{StageColl}レイヤーのみの指定で、@<code>{transform.forward}方向にレイを飛ばしてヒット確認しています。
ヒットがあればゲート操作の処理を行います。
まずは既存のゲートがあるのか確認し、なければ生成します。ペアリングもここで行います。
あとで使用するためにレイがぶつかったコライダーを@<code>{PortalGate.hitColl}にセットし、位置と向きを求めます。
位置はぶつかった平面から少し法線方向に浮かせてZファイティング対策をしています。
向きの求め方が少し変なのにお気づきでしょうか？
Quaternion.LookRotation()のアップベクトルの指定をnormal.yの正負で変えています。
通常はtransform.upでよいのですが、天井にゲートを出したときこのままだと前後（PortalGateのY方向）が反転して違和感があるのでこのようにしました。
たしかオリジナルのPortalでもこのような挙動だったと思います。

//image[fuqunaga/ceil_fall_bad][アップベクトル処理をしない場合]
//image[fuqunaga/ceil_fall_good][アップベクトル処理をした場合]


== VirtualCamera

=== 初期化

ゲートが開くとペアになった別のゲート（以降ペアゲートと呼びます）の向こう側が見えるのでこの描画をなんとか実装する必要があります。
「向こう側」を描画するための@<b>{「別のカメラ(VirtualCamera)を用意しRenderTextureにキャプチャ、それをPortalGateに貼り付けてメインカメラで描画」}というアプローチにしました。

VirtualCameraはゲートの向こう側の絵をキャプチャするカメラです。

//image[fuqunaga/virtual_camera][VirtualCamera]


@<code>{PortalGate.OnWillRenderObject()}が各カメラごとに呼ばれるのでそのタイミングでVirtualCameraが必要であれば生成します。

//emlist[PortalGate.cs]{
private void OnWillRenderObject()
{
～省略～
    VirtualCamera pairVC;
    if (!pairVCTable.TryGetValue(cam, out pairVC))
    {
        if ((vc == null) || vc.generation < maxGeneration)
        {
            pairVC = pairVCTable[cam] = CreateVirtualCamera(cam, vc);
            return;
        }
    }

～省略～
}
//}

ゲート同士を向かい合わせにすると向こう側の景色にもまたゲートが映り、そのゲートの向こう側にもまたゲートが・・・と、あわせ鏡のように無限に続きます。


//image[fuqunaga/gate_inf][向かい合わせのゲート][scale=0.5]


この場合、

 1. メインカメラに映るゲートの向こう側の絵を用意するVirtualCamera
 2. そのVirtualCameraに映るゲートの用の第二世代VirtualCamera
 3. さらにそのVirtualCameraに映るゲート用の第三世代VirtualCamera
 4. さらに・・・

 と愚直に実装するとVirtualCameraも無限に必要になってしまいます。
 さすがにそうはいかないので@<code>{PortalGate.maxGeneration}で世代数を制限し、
 それ以上は正確な絵ではないものの１フレーム前のテクスチャをゲートに貼り付けることで代用します。


//emlist[PortalGate.cs]{
VirtualCamera CreateVirtualCamera(Camera parentCam, VirtualCamera parentVC)
{
    var rootCam = parentVC?.rootCamera ?? parentCam;
    var generation = parentVC?.generation + 1 ?? 1;

    var go = Instantiate(virtualCameraPrefab);
    go.name = rootCam.name + "_virtual" + generation;
    go.transform.SetParent(transform);

    var vc = go.GetComponent<VirtualCamera>();
    vc.rootCamera = rootCam;
    vc.parentCamera = parentCam;
    vc.parentGate = this;
    vc.generation = generation;

    vc.Init();

    return vc;
}
//}

@<code>{VirtualCamera.rootCamera}はVirtualCameraの世代をさかのぼって大元になるメインカメラです。
他に、親のカメラ、対象のゲート、世代などを設定しています。

//emlist[VirtualCamera.cs]{
public void Init()
{
    camera_.aspect = rootCamera.aspect;
    camera_.fieldOfView = rootCamera.fieldOfView;
    camera_.nearClipPlane = rootCamera.nearClipPlane;
    camera_.farClipPlane = rootCamera.farClipPlane;
    camera_.cullingMask |= LayerMask.GetMask(new[] { PlayerLayerName });
    camera_.depth = parentCamera.depth - 1;

    camera_.targetTexture = tex0;
    currentTex0 = true;
}
//}

@<code>{VirtualCamera.Init()}で親のカメラからパラメータを引き継いでいます。
VirtualCameraには自キャラを映すのでCullingMaskから@<b>{Player}レイヤーを削除しています。
また親のカメラより先に絵をキャプチャしたいので@<code>{parentCamera.depth - 1}しています。

当初@<code>{Camera.CopyFrom()}を使っていたのですがどうもCommandBufferもコピーしてしまうようで、ポストエフェクトに使っているPostProcessingStack@<fn>{pp}との併用でエラーが出てしまったので各プロパティごとにコピーするようにしました。

//footnote[pp][https://github.com/Unity-Technologies/PostProcessing]


=== 更新

VirtualCameraは処理が軽いほど@<code>{PortalGate.maxGeneration}を多くできるのでできるだ無駄な処理をしないよう少しパフォーマンスに気を使っています。

//emlist[VirtualCamera.cs]{
private void LateUpdate()
{
    // PreviewCameraなどはこのタイミングでnullになっているようなのでチェック
    if (parentCamera == null)
    {
        Destroy(gameObject);
        return;
    }

    camera_.enabled = parentGate.IsVisible(parentCamera);
    if (camera_.enabled)
    {
        var parentCamTrans = parentCamera.transform;
        var parentGateTrans = parentGate.transform;

        parentGate.UpdateTransformOnPair(
            transform, 
            parentCamTrans.position, 
            parentCamTrans.rotation
            );


        UpdateCamera();
    }
}
//}

こちらのコードを詳しく追っていきます。

==== カメラの無効化

親のカメラにゲート映っていなければ向こう側の絵を用意する必要がないのでVirtualCameraのカメラを無効にします。

//emlist[PortalGate.cs]{
public bool IsVisible(Camera camera)
{
    var ret = false;

    var pos = transform.position;
    var camPos = camera.transform.position;

    var camToGateDir = (pos - camPos).normalized;
    var dot = Vector3.Dot(camToGateDir, transform.forward);
    if (dot > 0f)
    {
        var planes = GeometryUtility.CalculateFrustumPlanes(camera);
        ret = GeometryUtility.TestPlanesAABB(planes, coll.bounds);
    }

    return ret;
}
//}

可視判定は次のようになっています。

 1. 向きの判定。ゲートがカメラの方を向いているかをチェックしています。カメラ→ゲート方向とゲートのZ+方向の内積の符号で判定しています。
 2. 視錐台とバウンディングボックスの可視判定。Unityにはまさのこのための関数が用意されておりそのまま使えます。ありがたや。


==== 位置と向きの更新

@<code>{parentGate.UpdateTransformOnPair()} で「親ゲートに対する親カメラの位置と向きから、親のペアのゲートに対する位置と向きを求めてtransformを更新する」処理をしています。


//emlist[PortalGate.cs]{
public void UpdateTransformOnPair(
    Transform trans, 
    Vector3 worldPos,
    Quaternion worldRot
    )
{
    var localPos = transform.InverseTransformPoint(worldPos);
    var localRot = Quaternion.Inverse(transform.rotation) * worldRot;

    var pairGateTrans = pair.transform;
    var gateRot = pair.gateRot;
    var pos = pairGateTrans.TransformPoint(gateRot * localPos);
    var rot = pairGateTrans.rotation * gateRot * localRot;

    trans.SetPositionAndRotation(pos, rot);
}
//}

実装はこのようになっており、

 1. ゲートのローカル座標系に直す
 1. gateRotでゲートの手前→奥の向き変換
 1. ペアのゲートのローカル座標として扱い
 1. ワールド座標系に変換する

という手順になっています。
gateRotは

//emlist{
public Quaternion gateRot { get; } = Quaternion.Euler(0f, 180f, 0f);
//}

と、Y軸で180度回転にしてますが、
Z値が反転すればいいので
 
//emlist{
public Quaternion gateRot { get; } =  Quaterion.Euler(180f, 0f, 0f);
//}
 
のような実装でも一応破綻はしないはずです。
ただゲートの手前と奥で上方向が反転するのでゲートを通過すると自キャラの頭が地面側になるなど、
やはり違和感が出てしまうのでY軸回転がよさそうです。

==== カメラパラメータの更新

//emlist[VirtualCamera.cs]{
void UpdateCamera()
{
    var pair = parentGate.pair;
    var pairTrans = pair.transform;
    var mesh = pair.GetComponent<MeshFilter>().sharedMesh;
    var vtxList = mesh.vertices
                  .Select(vtx => pairTrans.TransformPoint(vtx)).ToList();

    TargetCameraUtility.Update(camera_, vtxList);

    // Oblique
    // pairGateの奥しか描画しない = nearClipPlane を pairGateと一致させる
    var pairGateTrans = parentGate.pair.transform;
    var clipPlane = CalcPlane(camera_, 
                              pairGateTrans.position,
                              -pairGateTrans.forward);

    camera_.projectionMatrix = camera_.CalculateObliqueMatrix(clipPlane);
}

Vector4 CalcPlane(Camera cam, Vector3 pos, Vector3 normal)
{
    var viewMat = cam.worldToCameraMatrix;

    var normalOnView = viewMat.MultiplyVector(normal).normalized;
    var posOnView = viewMat.MultiplyPoint(pos);

    return new Vector4(
        normalOnView.x,
        normalOnView.y,
        normalOnView.z,
        -Vector3.Dot(normalOnView, posOnView)
        );
}
//}


VirtualCameraはできるだけ処理を軽くしたいので視錐台もできるだけ狭くします。
VirtualCamera越しに見たペアゲートの範囲だけ描画できればいいので、
ペアゲートのメッシュの頂点をワールド座標にし、@<code>{TargetCameraUtility.Update()}でその頂点群が収まるように視錐台と@<code>{Camera.rect}を変更しています。

#@#図

またVirtualCameraとペアゲートの間のオブジェクトは描画しないのでカメラのニアクリップ面をペアゲートと同じ平面にします。
この操作は@<code>{Camera.CalculateObliqueMatrix()}で行えます。
あまりドキュメントがないのでサンプルコードなどからの判断になりますがニアクリップ面をビュー座標系でxyzに法線、wに距離をいれたVector4で渡すようです。


== ゲートの描画

状態に合わせて描画するものが違うのですが単一のシェーダーでやりきっています。

 * 枠と中身の表示がある
 * ゲートが生成、移動された直後は円が広がるアニメーションをする 
 * まだペアゲートが無いときは背景（既存の壁や床）をモヤモヤさせる（@<img>{fuqunaga/gate_no_pair}）
 * ペアゲートができたらモヤモヤからVirtualCameraの絵にフェードインする
 * @<code>{PortalGate.maxGeneration}に達してVirtualCameraが無い場合は１フレーム前の絵をPortalGateに貼り付ける

//image[fuqunaga/gate_no_pair][ペアゲートが無いときは背景をモヤモヤ][scale=0.5]

//emlist[PortalGate.shader]{
GrabPass
{
    "_BackgroundTexture"
}
//}

まずはGrabPass@<fn>{grabpass}で背景をキャプチャしときます。


//footnote[grabpass][https://docs.unity3d.com/ja/current/Manual/SL-GrabPass.html]


=== 頂点シェーダー

//emlist[PortalGate.shader]{
v2f vert(appdata_img In)
{
    v2f o;

    float3 posWorld = mul(unity_ObjectToWorld, float4(In.vertex.xyz, 1)).xyz;
    float4 clipPos = mul(UNITY_MATRIX_VP, float4(posWorld, 1));
    float4 clipPosOnMain = mul(_MainCameraViewProj, float4(posWorld, 1));

    o.pos = clipPos;
    o.uv = In.texcoord;
    o.sposOnMain = ComputeScreenPos(clipPosOnMain);
    o.grabPos = ComputeGrabScreenPos(o.pos);
    return o;
}
//}

頂点シェーダーはこんな感じです。
スクリーン座標系の位置を2つ求めていて、現在のカメラでの位置@<code>{clipPos}と、メインカメラのもの@<code>{clipPosOnMain}があります。
前者は通常のレンダリングに用い、後者はVirtualCameraでキャプチャしたRenderTextureを参照する際に使用します。
またGrabPassを用いるときは専用のポジション計算関数がありますのでこれを使います。


=== フラグメントシェーダー


//emlist[PortalGate.shader]{
float2 uv = In.uv.xy;
uv = (uv - 0.5) * 2; // map 0~1 to -1~1
float insideRate = (1 - length(uv)) * _OpenRate;
//}

@<code>{insideRate}（円の内側率）を求めています。
円の中心が1、円周上が0、それより外はマイナスになります。
@<code>{_OpenRate}で円の開き具合を変えれます。@<b>{PortalGate.Open()}で制御しています。


//emlist[PortalGate.shader]{
// background
float4 grabUV = In.grabPos;
float2 grabOffset = float2(
    snoise(float3(uv, _Time.y     )),
    snoise(float3(uv, _Time.y + 10))
);
grabUV.xy += grabOffset * 0.3 * insideRate;
float4 bgColor = tex2Dproj(_BackgroundTexture, grabUV);
//}

モヤモヤした背景を生成しています。
@<code>{snoise}はincludeしているNoise.cgincで定義されている関数でSimplexNoiseです。
uv値と時間でgrabUVを揺らしています。insideRateも乗算することで中心付近ほど揺らぎを大きくしています。


//emlist[PortalGate.shader]{
// portal other side
float2 sUV = In.sposOnMain.xy / In.sposOnMain.w;
float4 sideColor = tex2D(_MainTex, sUV);
//}

ゲートの向こう側の絵です。@<code>{_MainTex}にはVirutualCameraがキャプチャしたテクスチャが入っており、メインカメラのUV値で参照しています。


//emlist[PortalGate.shader]{
// color
float4 col = lerp(bgColor, sideColor, _ConnectRate);
//}

@<code>{bgColor}（壁や床）と@<code>{sideColor}（ゲートの向こう側）をミックスしています。
@<code>{_ConnectRate}はペアゲートができると0から1に遷移して、その後はずっと1のままです。

//emlist[PortalGate.shader]{
// frame
float frame = smoothstep(0, 0.1, insideRate);
float frameColorRate = 1 - abs(frame - 0.5) * 2;
float mixRate = saturate(grabOffset.x + grabOffset.y);
float3 frameColor = lerp(_FrameColor0, _FrameColor1, mixRate);
col.xyz = lerp(col.xyz, frameColor, frameColorRate);

col.a = frame;
//}

最後に枠を計算しています。
@<code>{insideRate}の端っこを、@<code>{_FrameColor0,_FrameColor1}を適当にミックスして表示しています。


ここまでで見た目は完成しました。次は物理的な挙動のほうに焦点をあわせてみます。


== オブジェクトのワープ

@<b>{PortalObjコンポーネント}でワープまわりの処理を行うようにしました。
これがついているGameObjectはワープできるようになります。

=== 既存コリジョンの無効化

ゲートが設置された平面はもともととおり抜けできない、つまりコリジョンがあります。
ゲートを通るときはこれを無効化しなくてはなりません。
実はゲートには前後にわりと大きめに飛び出たコライダーをトリガーとして付けています。
PortalObjはこのコライダーをトリガーとして平面とのコリジョン無効化を行っています。

//image[fuqunaga/gate_collision][ゲートのコライダー][scale=0.5]


//emlist[PortalObj.cs]{
private void OnTriggerStay(Collider other)
{
    var gate = other.GetComponent<PortalGate>();
    if ((gate != null) && !touchingGates.Contains(gate) && (gate.pair != null))
    {
        touchingGates.Add(gate);
        Physics.IgnoreCollision(gate.hitColl, collider_, true);
    }
}

private void OnTriggerExit(Collider other)
{
    var gate = other.GetComponent<PortalGate>();
    if (gate != null)
    {
        touchingGates.Remove(gate);
        Physics.IgnoreCollision(gate.hitColl, collider_, false);
    }
}
//}

@<code>{OnTriggerEnder()}ではなく@<code>{OnTriggerStay()}なのは、まだゲートがひとつでペアがない状態のときにEnterしその後ペアができたときにも対応するためです。
まずはトリガーとなったゲートを@<code>{tougingGates}に登録しておきます。
前述の@<code>{PortalGate.hitColl}がやっと出てきました。
これと自身のコライダーを@<code>{Physics.IgnoreCollision()}で衝突無視するようにセットしておきます。

@<code>{OnTriggerExit()}で衝突を有効に戻しています。
お気づきの方も多いかと思いますが、@<code>{PortalGate.hitColl}は平面全体のコライダーなので実はPortalGateの枠外でも通り抜けできてしまいます。
「OnTriggerStay()をキープしている限り」という条件はつくのであまり目立ちませんがちゃんとしたゲートの形でのコリジョンするにはもうちょっと複雑な処理が必要そうです。


=== ワープ処理

//emlist[PortalObj.cs]{
private void Update()
{
    var passedGate = touchingGates.FirstOrDefault(gate =>
    {
        var posOnGate = gate.transform.InverseTransformPoint(center.position);
        return posOnGate.z > 0f;
    });


    if (passedGate != null)
    {
        PassGate(passedGate);
    }

    if ((rigidbody_ != null) && !rigidbody_.useGravity)
    {
        if ((Time.time - ignoreGravityStartTime)  > ignoreGravityTime)
        {
            rigidbody_.useGravity = true;
        }
    }
}
//}

@<code>{center}はゲートを通過したかどうかの判定に使うTransformです。基本的にはPortalObjコンポーネントのついているGameObjectのものでいいのですが、
自キャラはキャラの中心ではなくカメラが通過した時点でワープしたいので手動で設定できるようにしています。
@<code>{center.position}が@<code>{z > 0f}（ゲートの裏）になっているゲートがないか@<code>{touchingGates}をチェックしています。
もしそのようなゲートが見つかれば@<code>{PassGate()}（ワープ処理）を行います。


また、後述しますがゲート通過直後にPortalObjは重力を無効化しています。
これは地面に落ちているオブジェクトの下に別の床に繋がるゲートを開くと、
オブジェクトがゲート間を行き来して振動してしまうので通過後は少し慣性をもったような挙動にするために行っています。


//emlist[PortalObj.cs]{
void PassGate(PortalGate gate)
{
    gate.UpdateTransformOnPair(transform);

    if (rigidbody_ != null)
    {
        rigidbody_.velocity = gate.UpdateDirOnPair(rigidbody_.velocity);
        rigidbody_.useGravity = false;
        ignoreGravityStartTime = Time.time;
    }

    if (fpController != null)
    {
        fpController.m_MoveDir = gate.UpdateDirOnPair(fpController.m_MoveDir);
        fpController.InitMouseLook();
    }
}
//}

ワープ処理はこんな感じになっています。
VirtualCameraの位置を求めるときにも使用した@<code>{PortalGate.UpdateTransformOnPair()}でTransformをワープさせます。
@<code>{RigidBody}を持っている場合は速度の向きも変えてやります。
@<code>{fpController}（自キャラ操作のスクリプト）も同様です。
この辺は大規模化するともっと対応が必要なオブジェクトが出てくるので各スクリプトコールバックを用意して通知するほうがいいかもしれません。


=== ワープの問題点

今回ワープを実装してみていくつかもうちょっと詰めていかないとなという点がありました。

==== PortalObjの速度がはやいと一度壁にぶつかる

物理エンジンが衝突判定をしたあと押出処理をする前になんらかの方法でコリジョンを無効化したかったのですがうまい方法が見つかりませんでした。
@<code>{OnTriggerEnter()},@<code>{OnCollisionEnter()}内で@<code>{Physics.IgnoreCollision()}を呼んでも一度衝突したあとから無効化されるようです。
おそらく@<code>{On~Enter()}が押出処理後に呼ばれているか@<code>{Physics.IgnoreCollision()}の反映されるタイミングが少し遅いのだと思います。
このためトリガーへEnterするフレームと壁に衝突するフレームが別になるようにトリガーの範囲をかなり大きく飛び出させています。
しかしこの方法では限界があり、より高速で移動するPortalObjには対応できていません。
もし「こういう方法あるよ！」という方がいましたらぜひご連絡いただきたいです！


==== 本当は途中にコピーを挟んだほうがよい

ワープを@<b>{「オブジェクトの位置を書き換える」}ことで実装しましたが厳密に考えればゲートをくぐってる最中は半分手前で半分向こう側という状態があるはずです。
大きいオブジェクトなどを出す場合は目立つのでこのあたりも考える必要がありそうです。
さらに手前と向こう両方の衝突物の影響も受ける必要があり、より厳密には物理エンジン内のソルバに介入しないといけなそうな気がします。
Unityだと厳しそうなのでうまくごまかす方向が現実的かなーという気がしています。


== まとめ

以前からやってみたかったPortalの再現をUnityで挑戦してみました。
カメラ重ねればいけるっしょーと気楽にはじめてみたものの思いのほか細かいところで大変なことがわかりました。
CGやゲーム技術のなかでもリアル寄りにするものは需要が高く定型化してどんどん手軽になっていっています。
現実感が簡単に出せるようになると、どこでもドアのような「いままでありがちだったけど現実味がなく眠っていたアイデア」が
今後は意外と新しい体験として活きてきたりするかもしれません。


== 参考
 * Portal @<href>{http://www.thinkwithportals.com/}
 * Adam Character Pack @<href>{https://assetstore.unity.com/packages/essentials/tutorial-projects/adam-character-pack-adam-guard-lu-74842}
 * playGROWnd @<href>{https://github.com/unity3d-jp/playgrownd}
 * PostProcessingStack @<href>{https://github.com/Unity-Technologies/PostProcessing}
