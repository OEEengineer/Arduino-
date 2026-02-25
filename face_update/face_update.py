import cv2
import numpy as np
import face_recognition
from flask import Flask, request
from flask_cors import CORS
import os
import dashscope
import base64
from dashscope import MultiModalConversation

# ================= 配置 =================
DASHSCOPE_API_KEY = os.environ.get("DASHSCOPE_API_KEY", "sk-ad5bf06369e7421db86c1c97bfc2a0d7")
dashscope.api_key = DASHSCOPE_API_KEY

# 人脸库路径
FACE_IMAGES = ["liu.jpg", "chen.jpg", "guo.jpg","lu.jpg","xie.jpg"]
FACE_NAMES = ["liu de hua", "chen yu lin", "guo fu cheng","lu xiang yu","xie yu hang"]

app = Flask(__name__)
CORS(app)  # 允许跨域


# ================= 加载已知人脸 =================
def load_known_faces():
    encodings = []
    names = []
    for path, name in zip(FACE_IMAGES, FACE_NAMES):
        if not os.path.exists(path):
            print(f"警告：图片 {path} 不存在")
            continue
        img = cv2.imread(path)
        if img is None:
            continue
        rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        faces = face_recognition.face_locations(rgb)
        if len(faces) == 0:
            print(f"警告：{path} 中未检测到人脸")
            continue
        enc = face_recognition.face_encodings(rgb, faces)[0]
        encodings.append(enc)
        names.append(name)
    return encodings, names


known_encodings, known_names = load_known_faces()
print(f"已加载 {len(known_encodings)} 个人脸")


# ================= 人脸识别函数 =================
def recognize_face(image_bytes):
    nparr = np.frombuffer(image_bytes, np.uint8)
    img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
    if img is None:
        return "Error"

    rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    rgb = np.ascontiguousarray(rgb)

    face_locations = face_recognition.face_locations(rgb)
    if len(face_locations) == 0:
        return "unknown"

    face_encodings = face_recognition.face_encodings(rgb, face_locations)
    if len(face_encodings) == 0:
        return "unknown"

    face_encoding = face_encodings[0]
    distances = face_recognition.face_distance(known_encodings, face_encoding)
    min_index = np.argmin(distances)
    # 阈值可根据实际情况调整，0.5 是默认值
    if distances[min_index] < 0.5:
        return known_names[min_index]
    else:
        return "unknown"


# ================= 通义千问物体识别函数 (修复版) =================
def recognize_object(image_bytes):
    """
    使用 Base64 编码图片，避免文件路径问题
    """
    try:
        # 1. 将图片字节转换为 Base64
        base64_image = base64.b64encode(image_bytes).decode('utf-8')
        image_data_uri = f"data:image/jpeg;base64,{base64_image}"

        # 2. 构建消息
        messages = [
            {
                "role": "user",
                "content": [
                    {"image": image_data_uri},
                    {"text": "What is the main object in this image? Answer with less than 5 English words."}
                ]
            }
        ]

        # 3. 调用 API
        response = MultiModalConversation.call(model="qwen3-vl-plus", messages=messages)

        # 4. 解析返回结果 (增加健壮性)
        if response.status_code == 200:
            content_list = response.output.choices[0].message.content
            # 兼容不同版本的返回结构
            text_result = ""
            if isinstance(content_list, list):
                for item in content_list:
                    if isinstance(item, dict) and "text" in item:
                        text_result = item["text"]
                        break
            elif isinstance(content_list, str):
                text_result = content_list

            # 清理结果
            result = text_result.strip().strip('.,!?;:"\'')
            return result
        else:
            print(f"API 错误：{response.code}, {response.message}")
            return "API Error"

    except Exception as e:
        print(f"物体识别异常：{e}")
        return "Error"


# ================= Flask 路由 =================
@app.route('/upload', methods=['POST'])
def upload_face():
    image_data = request.get_data()
    if not image_data:
        return "No image", 400
    result = recognize_face(image_data)
    return result, 200


@app.route('/upload_object', methods=['POST'])
def upload_object():
    image_data = request.get_data()
    if not image_data:
        return "No image", 400
    result = recognize_object(image_data)
    return result, 200


if __name__ == '__main__':
    # threaded=True 允许同时处理多个请求
    app.run(host='0.0.0.0', port=5000, debug=False, threaded=True)
