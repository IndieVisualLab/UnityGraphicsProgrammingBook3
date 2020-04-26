= Baking Skinned Animation to Texture

== はじめに

こんにちは、すぎのです！
この章では、数千、数万のスキニングアニメーションされたオブジェクトを表示します。

//image[birds][羽ばたきアニメーションをする鳥の群れ]{
//}

Unityでは、キャラクターアニメーションを実現するとき、AnimatorコンポーネントとSkinnedMeshRendererコンポーネントを使うことになると思います。

たとえば、鳥の群れや群衆を表現したいとき、どうするでしょう？数千、数万のキャラクターオブジェクトに対してAnimatorとSkinnedMeshRendererを使用するでしょうか。
一般的に、大量のオブジェクトを画面上に表示するとき、GPUインスタンシングを使用し、一度にまとめて大量のオブジェクトをレンダリングします。
しかし、SkinnedMeshRendererはインスタンシングをサポートしておらず、個別のオブジェクトを1つづつレンダリング処理することになり、処理が非常に重たくなってしまいます。

これを解決するソリューションとして、アニメーションされた頂点位置情報をテクスチャとして保存する方法があるのですが、実際にどういう方法か、実装までの考え方と応用、注意すべき点についてこの章では解説していきます。

説明を省いていたり、分かりにくいところもあるかと思いますので、お気軽にTwitter（@sugi_cho宛）でご質問ください。もし、間違っているところとかあったら、ご指摘いただけると助かります(._.)

== アニメーションするSkinedMeshRendererを5000体置いてみる

まずは、普通にアニメーションするオブジェクトを大量に（5000体）置いたらどれくらい処理が重いのか、見ていきたいと思います。
今回は、頂点数1890の、シンプルなアニメーション付き馬の3Dオブジェクトを用意しました。

//image[uma_model][使用した馬のモデル]{
//}

実際に動かしてみたところ、FPSは8.8、かなり処理が重くなっていることがわかります。@<img>{uma5000}

//image[uma5000][5000頭のアニメーションする馬]{
//}

では、この処理の中でどこが重くなっているのか、Unityのプロファイラを見て探っていくことにします。
WindowメニューからProfiler（ショートカットキー: Ctr+7）を表示します。Add Profilerのプルダウンから、GPUを選択し、GPU Usageのプロファイラを表示するとより詳細な情報を得られます。
GPU Usageの情報を取得すること自体がオーバーヘッドになるので、必要ない時は表示しない方がいいのですが、今回はGPU Usageが重要になってくるので積極的に使用します。

//image[profiler][Profiler Window (GPU Usage)]{
//}

プロファイラを見ると、CPUの処理時間よりもGPUの処理時間の方が多く、CPUではGPUの処理完了待ちが発生していることが分かります。@<img>{profiler}
そして、GPU処理の約7割を@<code>{PostLateUpdate.UpdateAllSkinnedMeshes}が占めていることが分かります。
また、見えている馬のオブジェクト数ぶん、@<code>{Camera.Renderer}が走っているので、オブジェクトのバッチングかGPUインスタンシングを行うことにより、GPUのレンダリング処理数を抑えることができそうです。
ちなみにですが、CPU Usageでも同じように@<code>{PostLateUpdate.UpdateAllSkinnedMeshes}と@<code>{Camera.Render}の処理がほとんどを占めています。

このテストシーンでは、PlayerSettingsでGPU Skinningを使用する設定になっています。もしもGPUでなくCPUでスキニングを行っていた場合、CPUの処理の割合が増大し、今よりもFPSが落ちることになります。
GPUスキニング時は、CPU側でボーンの行列の計算を行い、行列情報をGPUに渡し、GPUでスキニング処理を行います。CPUスキニング時は、行列の計算とスキニングの処理までCPU側で行い、スキニングされた頂点データをGPU側に渡しています。

このように、処理の最適化には最初にどこが処理のボトルネックになっているか、見極めることが重要になってきます。

== SkinnedMeshRendererのアニメーションする頂点の位置をあらかじめ計算しておく

プロファイリングの結果、メッシュのスキニングの処理が重そうだ。ということが分かったので、スキニングの処理自体はリアルタイムでは行わずに、あらかじめ計算しておく方法を検討してみようと思います。

@<code>{SkinnedMeshRenderer}のスキニング処理された後の頂点情報を取得する方法としては、@<code>{SkinnedMeshRenderer.BakeMesh(Mesh)}という関数があります。これは、スキニングされた状態のメッシュのスナップショットを作成し、指定したメッシュに格納します。
少々、処理に時間がかかるのですが、スキニングされた頂点情報を事前に格納しておくという使い方のためなら、選択可能な方法といえます。

//listnum[SkinnedMeshRenderer.BakeMesh][SkinnedMeshRenderer.BakeMesh() Example]{
Animator animator;
SkinnedMeshRenderer skinedMesh;
List<Mesh> meshList;

void Start(){
    animator = GetComponent<Animator>();
    skinnedMesh = GetComponentInChildren<SkinnedMeshRenderer>();
    meshList = new List<Mesh>();
    animator.Play("Run");
}

void Update(){
    var mesh = new Mesh();
    skinnedMesh.BakeMesh(mesh);
    //mesh内に、スキニングされたメッシュのスナップショットが格納される
    meshList.Add(mesh);
}
//}

これで、AnimatorのRunステートのアニメーションの@<code>{SkinnedMeshRenderer}の毎フレームのスナップショットのMeshがmeshListに格納されていきます。@<list>{SkinnedMeshRenderer.BakeMesh}

この保存された@<code>{meshList}を使い、パラパラ漫画で絵を切り替える要領でにメッシュ（@<code>{MeshFilter.sharedMesh}）を切り替えていくと、@<code>{SkinnedMeshRenderer}を使用せずにメッシュのアニメーションが表示できるので、プロファイリングした結果ボトルネックになっていたスキニングの処理を省くことができそうです。

== 頂点情報を保存する方法の検討

しかし、このMeshデータをフレームごとに複数保存しておく実装だとアニメーションによって変更しないメッシュの情報（Mesh.indeces, Mesh.uv等）も保存することになり、無駄が多くなります。
スキニングアニメーションの場合、更新されるデータは頂点位置の情報と法線情報だけなので、これらのみを保存し、更新していけばいいのです。

=== 頂点情報をVector3配列で保存しておく方法

そこで考えられるのが、フレームごとの頂点位置と法線データをVector3の配列で持っておいて、フレームごとにメッシュの位置と法線を更新していく方法です。@<list>{mesh.SetVertices}

//listnum[mesh.SetVertices][Update Mesh]{
Mesh objMesh;
List<Vector3>[] vertecesLists;
List<Vector3>[] normalsLists;
//保存しておいた頂点の情報
//Mesh.SetVertices(List<Vector3>)で使うため

void Start(){
    objMesh = GetComponent<MeshFilter>().mesh;
    objMesh.MarkDynamic();
}

void Update(){
    var frame = xx; 
    //現在時刻でのフレームを計算する

    objMesh.SetVertices(vertecesLists[frame]);
    objMesh.SetNormals(normalsLists[frame]);
}
//}

しかし、この方法だと今解決しようとしている数千のアニメーションオブジェクトを表示するという目的に対して、メッシュの更新自体のCPUの処理負荷が大きくなってしまいます。

そこで、この章の冒頭から答えは書いてあるのですが、テクスチャに位置情報と法線情報を格納し、VertexTextureFetchを用いて頂点シェーダにおいてメッシュの頂点位置と法線情報を更新します。
これにより元のメッシュデータ自体の更新は行う必要がなくなるのでCPUの処理負荷無しで頂点アニメーションを実現可能になります。

=== 位置情報をテクスチャに書き込む

それでは、メッシュ頂点の位置情報をテクスチャに保存する手法について、軽く説明しようと思います。

Unityの@<code>{Mesh}オブジェクトは、Unityで表示する3Dモデルの頂点の位置、法線、UV値などのデータが格納されたクラスになっています。
頂点位置情報（@<code>{Mesh.vertices}）には、メッシュの全ての頂点数分の位置情報が@<code>{Vector3}の配列で保存されています。@<table>{vec3}

そして、Unityの@<code>{Texture2D}オブジェクトは、テクスチャの幅(@<code>{texture.width})×高さ（@<code>{texture.height}）のピクセル数分、色情報(@<code>{Color})の配列で保存されています。@<table>{col}

//table[vec3][位置情報（Vector3）]{
x   float   x方向成分
y   float   y方向成分
z   float   z方向成分
//}

//table[col][色情報（Color）]{
r   float   赤色成分
g   float   緑色成分
b   float   青色成分
a   float   不透明度成分
//}

頂点の位置、Mesh.vertices@<table>{vec3}のx,y,zの値をそれぞれ、Texture2Dの色情報@<table>{col}のr,g,bに格納し、EditorScriptでTextureAssetとして保存すれば、頂点の位置情報をテクスチャとして保存することになります。
メッシュ頂点の位置と法線をテクスチャの色として保存する、サンプルスクリプトです。@<list>{vec2col}

//listnum[vec2col][頂点情報をテクスチャに保存する]{
public void CreateTex(Mesh sourceMesh)
{
    var vertCount = sourceMesh.vertexCount;
    var width = Mathf.FloorToInt(Mathf.Sqrt(vertCount));
    var height = Mathf.CeilToInt((float)vertCount / width);
    //頂点数＜幅×高さになるwidth,heightを求める

    posTex = new Texture2D(width, height, TextureFormat.RGBAFloat, false);
    normTex = new Texture2D(width, height, TextureFormat.RGBAFloat, false);
    //Color[]を格納するTexture2D
    //TextureFormat.RGBAFloatを指定することで、色情報を各要素Float値で持てる

    var vertices = sourceMesh.vertices;
    var normals = sourceMesh.normals;
    var posColors = new Color[width * height];
    var normColors = new Color[width * height];
    //頂点数分の色情報配列

    for (var i = 0; i < vertCount; i++)
    {
        posColors[i] = new Color(
            vertices[i].x, 
            vertices[i].y, 
            vertices[i].z
        );
        normColors[i] = new Color(
            normals[i].x, 
            normals[i].y, 
            normals[i].z
        );
    }
    //各頂点において、Color.rgb = Vector3.xyzとなるように、
    //位置→色、法線→色となるような色配列(Color[])を生成する。

    posTex.SetPixels(posColors);
    normTex.SetPixels(normColors);
    posTex.Apply();
    normTex.Apply();
    //色配列をテクスチャにセットし、適用する
}
//}

これで、@<code>{Mesh}の頂点の位置、法線情報を位置テクスチャ、法線テクスチャに焼きこむことができました。

//image[mesh2tex][Meshの頂点位置と法線をTextureに書き込み]{
//}

実際には、ポリゴンを作る時の頂点の並び順（Index）データが無いので位置テクスチャと法線テクスチャのみではメッシュの形を再現することはできないのですが、メッシュの情報をテクスチャに書き込むことができました。@<img>{mesh2tex}

Unityの公式マニュアルでは、@<code>{Texture2D.SetPixels(Color[])}は、@<code>{ColorFormat.RGBA32,ARGB32,RGB24,Alpha8}の場合のみ動作する。と書いてあります。
これは、固定小数点値、Fixed精度のテクスチャフォーマット時のみということなのですが、どうやら@<code>{RGBAHalf, RGBAFloat}の浮動小数点値でも、動作するようで、
色の各要素に負の値や1以上の値を代入しても、@<code>{Clamp}されずに値を保持してくれるようです。固定精度のテクスチャに@<code>{Color}を代入すると、RGB値は０～１の値、精度は1/256に制限されます。

今回のアニメーションの頂点情報をテクスチャに焼きこむ手法では、アニメーションを一定間隔ごとにサンプリングし、各フレームのメッシュの頂点情報を並べて、一連のアニメーション情報をまとめて１枚のテクスチャに焼きこみます。
テクスチャは、位置情報テクスチャと法線情報テクスチャの計２枚、生成します。

=== AnimationClip.SampleAnimation()

今回、アニメーションのサンプリングには、@<code>{AnimationClip.SampleAnimation(gameObject, time);}という関数を使用します。指定したGameObjectに対して、AnimationClipの指定した時間の状態にする。
というもので、レガシー@<code>{Animation}にも、@<code>{Animator}にも対応しています。（というより、AnimationやAnimatorのコンポーネントを使用せずにアニメーションを再生する方法です。）

それでは、実際の、AnimationClipからフレームを指定し頂点位置を取得する実装を解説していきます。

== 実装

今回のプログラムは次の３つの要素から成り立っています。

 * AnimationClipTextureBaker.cs
 * MeshInfoTextureGen.compute
 * TextureAnimPlayer.shader

AnimationClipTextureBakerで、Animation、もしくはAnimatorからAnimationClipを取得し、AnimationClipを各フレームにサンプリングしながらのメッシュの頂点データのComputeBufferを作成します。
そして、AnimationClipとMeshのデータから作られた頂点アニメーション情報のComputeBufferをMeshInfoTextureGen.computeにて、位置情報テクスチャと法線情報テクスチャに変換するComputeShaderです。

TextureAnimPlayer.shaderは、作られた位置情報テクスチャと法線情報テクスチャを使ってメッシュをアニメーションさせるためのShaderになります。

//image[animBakerInspector][AnimationClipTextureBaker Inspector]{
//}

@<code>{AnimationClipTextureBaker}のインスペクタです。アニメーションテクスチャを生成するための@<code>{ComputeShader}、アニメーションテクスチャを再生するための@<code>{Shader}を設定します。
そして、テクスチャ化したい@<code>{AnimationClip}を@<code>{Clips}に設定しておきます。@<img>{animBakerInspector}

//image[animBakerInspector2][インスペクタのコンテキストメニューからテクスチャの書き込みを実行できる]{
//}

@<code>{ContextMenuAttribute}を使用すると、スクリプト内のメソッドをUnityのインスペクタのコンテキストメニューから呼べるようになります。
エディター拡張を作らなくても実行できるので便利です。今回の場合、コンテキストメニューの『bake texture』から、スクリプトの@<code>{Bake}を呼び出せます。@<img>{animBakerInspector}

それでは、実際のコードを見ていきましょう。

//listnum[AnimationClipTextureBaker][AnimationClipTextureBaker.cs]{
using System.Collections.Generic;
using System.Linq;
using UnityEngine;

#if UNITY_EDITOR
using UnityEditor;
using System.IO;
#endif

public class AnimationClipTextureBaker : MonoBehaviour
{

    public ComputeShader infoTexGen;
    public Shader playShader;
    public AnimationClip[] clips;

//頂点情報は位置と法線の構造体
    public struct VertInfo
    {
        public Vector3 position;
        public Vector3 normal;
    }

//Reset()は、エディタ上でGameObjectにスクリプトを付けるときに呼ばれる
    private void Reset()
    {
        var animation = GetComponent<Animation>();
        var animator = GetComponent<Animator>();

        if (animation != null)
        {
            clips = new AnimationClip[animation.GetClipCount()];
            var i = 0;
            foreach (AnimationState state in animation)
                clips[i++] = state.clip;
        }
        else if (animator != null)
            clips = animator.runtimeAnimatorController.animationClips;
//Animation、Animatorのコンポーネントがあったら自動的にAnimationClipを設定する
    }

    [ContextMenu("bake texture")]
    void Bake()
    {
        var skin = GetComponentInChildren<SkinnedMeshRenderer>();
        var vCount = skin.sharedMesh.vertexCount;
        var texWidth = Mathf.NextPowerOfTwo(vCount);
        var mesh = new Mesh();

        foreach (var clip in clips)
        {
            var frames = Mathf.NextPowerOfTwo((int)(clip.length / 0.05f));
            var dt = clip.length / frames;
            var infoList = new List<VertInfo>();

            var pRt = new RenderTexture(texWidth, frames, 
                0, RenderTextureFormat.ARGBHalf);
            pRt.name = string.Format("{0}.{1}.posTex", name, clip.name);
            var nRt = new RenderTexture(texWidth, frames, 
                0, RenderTextureFormat.ARGBHalf);
            nRt.name = string.Format("{0}.{1}.normTex", name, clip.name);
            foreach (var rt in new[] { pRt, nRt })
            {
                rt.enableRandomWrite = true;
                rt.Create();
                RenderTexture.active = rt;
                GL.Clear(true, true, Color.clear);
            }
            //テクスチャの初期化

            for (var i = 0; i < frames; i++)
            {
                clip.SampleAnimation(gameObject, dt * i);
//AnimationClipの指定した時間の状態でGameObjectをサンプリング
                skin.BakeMesh(mesh);
//BakeMesh()を呼んでSkinningされた状態のメッシュデータを取得

                infoList.AddRange(Enumerable.Range(0, vCount)
                    .Select(idx => new VertInfo()
                    {
                        position = mesh.vertices[idx],
                        normal = mesh.normals[idx]
                    })
                );
//アニメーションのフレームを先にリストに格納しておく
            }
            var buffer = new ComputeBuffer(
                infoList.Count, 
                System.Runtime.InteropServices.Marshal.SizeOf(
                    typeof(VertInfo)
                )
            );
            buffer.SetData(infoList.ToArray());
//頂点情報をComputeBufferにセット

            var kernel = infoTexGen.FindKernel("CSMain");
            uint x, y, z;
            infoTexGen.GetKernelThreadGroupSizes(
                kernel, 
                out x, 
                out y, 
                out z
            );

            infoTexGen.SetInt("VertCount", vCount);
            infoTexGen.SetBuffer(kernel, "Info", buffer);
            infoTexGen.SetTexture(kernel, "OutPosition", pRt);
            infoTexGen.SetTexture(kernel, "OutNormal", nRt);
            infoTexGen.Dispatch(
                kernel, 
                vCount / (int)x + 1, 
                frames / (int)y + 1, 
                1
            );
//ComputeShaderをセットアップし、テクスチャ生成する

            buffer.Release();

//生成したテクスチャを保存するためのエディタースクリプト
#if UNITY_EDITOR
            var folderName = "BakedAnimationTex";
            var folderPath = Path.Combine("Assets", folderName);
            if (!AssetDatabase.IsValidFolder(folderPath))
                AssetDatabase.CreateFolder("Assets", folderName);

            var subFolder = name;
            var subFolderPath = Path.Combine(folderPath, subFolder);
            if (!AssetDatabase.IsValidFolder(subFolderPath))
                AssetDatabase.CreateFolder(folderPath, subFolder);

            var posTex = RenderTextureToTexture2D.Convert(pRt);
            var normTex = RenderTextureToTexture2D.Convert(nRt);
            Graphics.CopyTexture(pRt, posTex);
            Graphics.CopyTexture(nRt, normTex);

            var mat = new Material(playShader);
            mat.SetTexture("_MainTex", skin.sharedMaterial.mainTexture);
            mat.SetTexture("_PosTex", posTex);
            mat.SetTexture("_NmlTex", normTex);
            mat.SetFloat("_Length", clip.length);
            if (clip.wrapMode == WrapMode.Loop)
            {
                mat.SetFloat("_Loop", 1f);
                mat.EnableKeyword("ANIM_LOOP");
            }

            var go = new GameObject(name + "." + clip.name);
            go.AddComponent<MeshRenderer>().sharedMaterial = mat;
            go.AddComponent<MeshFilter>().sharedMesh = skin.sharedMesh;
//生成したテクスチャをマテリアルに設定して、メッシュを設定しPrefabを作っている

            AssetDatabase.CreateAsset(posTex, 
                Path.Combine(subFolderPath, pRt.name + ".asset"));
            AssetDatabase.CreateAsset(normTex, 
                Path.Combine(subFolderPath, nRt.name + ".asset"));
            AssetDatabase.CreateAsset(mat, 
                Path.Combine(subFolderPath, 
                string.Format("{0}.{1}.animTex.asset", name, clip.name)));
            PrefabUtility.CreatePrefab(
                Path.Combine(folderPath, go.name + ".prefab")
                .Replace("\\", "/"), go);
            AssetDatabase.SaveAssets();
            AssetDatabase.Refresh();
#endif
        }
    }
}
//}

いったん@<code>{RenderTexture}を生成してGPUで処理し、@<code>{Graphics.CopyTexture(rt,tex2d);}で@<code>{Texture2D}にコピーし、
エディタースクリプトでUnityAssetとして保存しておけば、次からは再計算無しで使えるアセットとなるので、使いどころの多いテクニックかと思います。
@<list>{AnimationClipTextureBaker}（119,120行目）

今回の実装では、テクスチャへの書き込みを@<code>{ComputeShader}を用いて実装しています。
大量の処理を行う場合、GPUを使用した方が高速になるので、有用なテクニックですので、ぜひマスターしてみてください。
処理内容としては、スクリプトで生成した頂点アニメーションの位置バッファ、法線バッファを各ピクセルにそのまま配置しているだけです。
@<list>{MeshInfoTextureGen}

//listnum[MeshInfoTextureGen][MeshInfoTextureGen.compute]{
#pragma kernel CSMain

struct MeshInfo{
	float3 position;
	float3 normal;
};

RWTexture2D<float4> OutPosition;
RWTexture2D<float4> OutNormal;
StructuredBuffer<MeshInfo> Info;
int VertCount;

[numthreads(8,8,1)]
void CSMain (uint3 id : SV_DispatchThreadID)
{
	int index = id.y * VertCount + id.x;
	MeshInfo info = Info[index];

	OutPosition[id.xy] = float4(info.position, 1.0);
	OutNormal[id.xy] = float4(info.normal, 1.0);
//テクスチャのx軸が頂点ID、y軸方向が時間になるように頂点情報を並べる
}
//}

スクリプトから生成されたテクスチャがこちらになります。@<img>{animTexes}

//image[animTexes][生成されたテクスチャ]{
//}

このテクスチャはx軸方向に１列ずつ、サンプリングした各フレームにおけるメッシュの頂点が格納されています。
そして、y軸方向が時間になっていて、テクスチャをサンプリングするときの@<code>{uv.y}を変化させることでアニメーションの時間を指定することができるよう、テクスチャを設計しています。

注目していただきたいことは@<code>{Texture.FilterMode = Bilinear}になっているトコロです。
テクスチャをサンプリングするとき各ピクセルが隣接するピクセルと補間されるのですが、これにより、テクスチャ生成時にスクリプトでサンプリングしたフレームと次のフレームの間の中途半端な時刻のアニメーションテクスチャをアニメーション再生時にShaderでサンプリングしたとき、
フレーム毎の位置と法線が自動的に補間されることになるので、アニメーションが滑らかに再生されることになります。ちょっと説明がややこしいですね！

そしてこの場合、走行（Run）のアニメーションはループアニメーションなので@<code>{WrapMode = Repeat}にしています。
これにより、アニメーションテクスチャの最後のピクセルと最初のピクセルが補間されるので、滑らかにループが繋がったアニメーションになります。
もちろん、ループしないアニメーションからテクスチャを生成する場合、@<code>{WrapMode = Clamp}に設定する必要があります。

次に、生成したアニメーションテクスチャを再生するためのShaderになります。@<list>{TextureAnimPlayer}

//listnum[TextureAnimPlayer][TextureAnimPlayer.shaer]{
Shader "Unlit/TextureAnimPlayer"
{
	Properties
	{
		_MainTex ("Texture", 2D) = "white" {}
		_PosTex("position texture", 2D) = "black"{}
		_NmlTex("normal texture", 2D) = "white"{}
		_DT ("delta time", float) = 0e

		_Length ("animation length", Float) = 1
		[Toggle(ANIM_LOOP)] _Loop("loop", Float) = 0
	}
	SubShader
	{
		Tags { "RenderType"="Opaque" }
		LOD 100 Cull Off

		Pass
		{
			CGPROGRAM
			#pragma vertex vert
			#pragma fragment frag
			#pragma multi_compile ___ ANIM_LOOP
//ループ用のマルチコンパイルを作っておくと便利

			#include "UnityCG.cginc"

			#define ts _PosTex_TexelSize

			struct appdata
			{
				float2 uv : TEXCOORD0;
			};

			struct v2f
			{
				float2 uv : TEXCOORD0;
				float3 normal : TEXCOORD1;
				float4 vertex : SV_POSITION;
			};

			sampler2D _MainTex, _PosTex, _NmlTex;
			float4 _PosTex_TexelSize;
			float _Length, _DT;

			v2f vert (appdata v, uint vid : SV_VertexID)
//SV_VertexIDのセマンティックで頂点IDを取得できる
			{
				float t = (_Time.y - _DT) / _Length;
#if ANIM_LOOP
				t = fmod(t, 1.0);
#else
				t = saturate(t);
#endif

				float x = (vid + 0.5) * ts.x;
				float y = t;
//uv.xは頂点IDを元に指定する
//uv.yにアニメーションをサンプリングする時間(t)を設定する

				float4 pos = tex2Dlod(
                    _PosTex, 
                    float4(x, y, 0, 0)
                );
				float3 normal = tex2Dlod(
                    _NmlTex, 
                    float4(x, y, 0, 0)
                );
//テクスチャから位置情報と法線情報をサンプリング

				v2f o;
				o.vertex = UnityObjectToClipPos(pos);
				o.normal = UnityObjectToWorldNormal(normal);
				o.uv = v.uv;
				return o;
			}

			half4 frag (v2f i) : SV_Target
			{
                half diff = dot(
                    i.normal, 
                    float3(0, 1, 0)
                ) * 0.5 + 0.5;
				half4 col = tex2D(_MainTex, i.uv);
				return diff * col;
			}
			ENDCG
		}
	}
}
//}

アニメーションテクスチャを再生するShaderでは、VertexTextureFetch（VTF）という手法を使用しています。簡単にいうと、頂点シェーダ内でテクスチャをサンプリングし、頂点の位置や各値の計算に使用する。という方法で、ディスプレイスメントマッピング等に良く利用されます。

テクスチャのサンプリングには頂点IDを使用しているのですが、これは、@<code>{SV_VertexID}のセマンティックで取得できます。頂点情報は位置情報も法線情報もテクスチャから取得するので、appdata内にはuvしか無い部分も注目です。（@<code>{appdata}に@<code>{POSITION,NORMAL}セマンティックを定義しても特にエラーにはなりません）

テクスチャをサンプリングするときのUVですが、@<code>{uv.y}がアニメーションの正規化した時間（アニメーションの始まりを0、終わりを1.0にしたときの値）になっています。
@<code>{uv.x}は、頂点インデックス（vid）、@<code>{uv.x = (vid + 0.5) * _TexelSize.x}となっていて、この0.5は何なのか？と思うかも知れないのですが、これはテクスチャを@<code>{Bilinear}でサンプリングしたとき、@<code>{(n + 0.5) / テクスチャサイズ}の位置だと、補間されていないテクスチャに入った値を得ることができるので、頂点IDに0.5の値を足して、メッシュ内の頂点同士の補間されていない位置や法線を取得しています。

//list[texelSize][{TextureName}_TexelSize テクスチャサイズの情報を含むfloat4のプロパティ(Unity公式マニュアルより)]{
x には 1.0/width が含まれます
y には 1.0/height が含まれます
z には width が含まれます
w には height が含まれます
//}

== アニメーションする馬を5000頭置いてみる

//image[uma50002][テクスチャによりアニメーションする5000頭の馬]{
//}

@<code>{SkinnedMeshRenderer}を使わず、@<code>{Renderer}とアニメーションテクスチャにより、アニメーションを再生しています。FPSはスキニングアニメーションを使用していたときと比べると8→56.4と、大きく改善しています。@<img>{uma50002}

※現在執筆中のPCのGPUは、GeForce MX150で、NVIDIA Pascal GPUの中でも最弱のものとなっています。プロファイラとゲームウィンドウを同時にキャプチャするため、レンダリング解像度が少し小さくなりましたが、処理負荷のほとんどがメッシュのスキニングの処理だったので、そこまでは影響ないはずです。。！

また、注目してほしいのはインスタンシング対応等、他の最適化処理はしてないというトコロです。@<code>{SkinnedMeshRenderer}を使わなくなったので、GPUインスタンシングによる描画が可能になりました。
Shaderのインスタンシング対応などにより、さらにパフォーマンスを追及することが可能だということです。

ここでは解説しませんが、表紙の鳥はテクスチャアニメーションさせた鳥を@<code>{Graphics.DrawMeshInstancedIndirect()}を用いて約4000羽の鳥を一気に描画しています。
Shaderのインスタンシング対応や他の応用については、ぼくのGitHubや他の記事を参考にしてみてください。

== 制限と応用先の検討

このテクスチャを使った手法にはいくつか制限があります。メッシュの頂点数やアニメーションの長さによりテクスチャを保持しておくメモリが消費される。
アニメーションのブレンドにはShaderを書く必要がある。AnimatorControllerのステートマシンを使用できない。などです。

その中で、一番大きな制限としては、ハードウェアごとに使用できるテクスチャの最大サイズがあります。それは、4Kだったり8K、16Kだったりします。
つまり、今回の手法ではメッシュの各フレームの頂点を横1列に並べるのでメッシュの頂点数がテクスチャサイズによって制限されるということです。

しかし、大量にオブジェクトを出す場合、そこまで頂点数の多いものを出すべきではないので、頂点数の制限は、そのまま受け入れ、メッシュの頂点数がテクスチャの最大サイズを超えないようにすることが得策かも知れません。
この頂点数の制限を超えてベイキングアニメーションテクスチャを使いたい場合、複数テクスチャを使用する方法が考えられます。

もしくは、メッシュの頂点ではなく、スケルトンの各ボーンの行列を事前に計算しておき、テクスチャやバッファに保存しておく方法もあります。
スキニングの処理自体は実行時にVertexShaderで行うことになるので、通常のメッシュスキニング時に@<code>{PostLateUpdate.UpdateAllSkinnedMeshes}で行われていたスキニングの処理を@<code>{Camera.Render}のレンダリング時にまとめて行うことになるので、処理負荷としてもかなり軽くなります。
ぜひ、試してみてください。

AnimatorControllerやUnityのステートマシンを使用できないので、アニメーションの制御が難しくなるので、メインキャラクターではなく、ループアニメーションを繰り返すモブキャラや大量に飛ぶ鳥や蝶の群れなど、ある程度ごまかしが効くもに応用するのが良いかと思います。

== まとめ

 * 処理の最適化を進めるときは適切にプロファイリングし、どこの処理が重いか見極めることが重要
 * スキニングアニメーションは処理負荷が高い
 * 事前にスキニングした頂点座標をテクスチャに保存しておくことにより、時の実行処理負荷を減らすことができる
 * GPUインスタンシングやGPUによるキャラクターの移動など、更なる最適化をする余地がある
 * スケルトンの行列やシミュレーション結果など、リアルタイムだと重い処理を事前にテクスチャに保存しておき、実行時の処理負荷を軽くする方法がいろいろな応用の余地がありそう
