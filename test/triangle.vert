/****************************************************************************
MIT License

Copyright (c) 2024 Guillaume Boissé

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
****************************************************************************/

struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

float3 Color1;
float3 Color2;
float Dist;

PSInput main(float3 pos : Position, uint iid : SV_InstanceID)
{
PSInput o;
    o.position = float4(pos.xy, iid == 0 ? 1.f : 0.5f, 1);
    o.color = float4(iid == 1 ? float3(1, 0, 0) : float3(0, 1, 0), 1);
return o;
}
