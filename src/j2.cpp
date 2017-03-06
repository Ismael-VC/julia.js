#include <j2.h>

#include <node.h>
#include <node_buffer.h>

v8::Local<v8::Object> NewTypedArray(v8::Isolate *isolate, const char *name,
                                    v8::Local<v8::Value> buffer,
                                    size_t byte_offset, size_t length) {
  v8::Local<v8::Function> constructor =
      isolate->GetCurrentContext()
          ->Global()
          ->Get(v8::String::NewFromUtf8(isolate, name))
          .As<v8::Function>();

  v8::Local<v8::Value> args[3] = {buffer,
                                  v8::Integer::New(isolate, byte_offset),
                                  v8::Integer::New(isolate, length)};
  return constructor->NewInstance(3, args);
}

static void
ArrayDescriptorConstructor(const v8::FunctionCallbackInfo<v8::Value> &info) {
  v8::Isolate *isolate = info.GetIsolate();

  info.This()->Set(v8::String::NewFromUtf8(isolate, "dims"), info[0]);
  info.This()->Set(v8::String::NewFromUtf8(isolate, "data"), info[1]);
}

static v8::Local<v8::Object> NewArrayDescriptor(v8::Isolate *isolate,
                                                jl_value_t *value) {
  v8::Local<v8::ObjectTemplate> instance =
      j2::array_descriptor.Get(isolate)->InstanceTemplate();
  v8::Local<v8::Object> res = instance->NewInstance();

  int32_t ndims = jl_array_ndims(value);
  v8::Local<v8::Array> dims = v8::Array::New(isolate, ndims);
  for (int32_t i = 0; i < ndims; ++i) {
    dims->Set(i, v8::Number::New(isolate, jl_array_size(value, i)));
  }
  res->Set(v8::String::NewFromUtf8(isolate, "dims"), dims);

  v8::Local<v8::Object> buffer =
      node::Buffer::New(isolate, static_cast<char *>(jl_array_data(value)),
                        jl_array_len(value) *
                            reinterpret_cast<jl_array_t *>(value)->elsize,
                        [](char *d, void *value) {}, value)
          .ToLocalChecked();
  res->Set(
      v8::String::NewFromUtf8(isolate, "data"),
      NewTypedArray(isolate, "Float32Array",
                    buffer->Get(v8::String::NewFromUtf8(isolate, "buffer")), 0,
                    jl_array_len(value)));

  return res;
}

v8::Persistent<v8::FunctionTemplate> j2::array_descriptor;

static void
ArrayDescriptorEquals(const v8::FunctionCallbackInfo<v8::Value> &info) {
  // ...
  printf("equals\n");
}

void j2::Init(v8::Isolate *isolate) {
  v8::Local<v8::FunctionTemplate> f =
      v8::FunctionTemplate::New(isolate, ArrayDescriptorConstructor);
  f->SetClassName(v8::String::NewFromUtf8(isolate, "ArrayDescriptor"));

  v8::Local<v8::Template> proto_t = f->PrototypeTemplate();
  proto_t->Set(isolate, "equals",
               v8::FunctionTemplate::New(isolate, ArrayDescriptorEquals));

  f->InstanceTemplate()->Set(v8::String::NewFromUtf8(isolate, "dims"),
                             v8::Null(isolate));

  array_descriptor.Reset(isolate, f);
}

jl_value_t *j2::FromJavaScriptArray(v8::Local<v8::Value> value) {
  return jl_nothing;
}

jl_value_t *j2::FromJavaScriptBoolean(v8::Local<v8::Value> value) {
  return jl_box_bool(value->ToBoolean()->Value());
}

jl_value_t *j2::FromJavaScriptString(v8::Local<v8::Value> value) {
  v8::String::Utf8Value s(value);
  return jl_pchar_to_string(*s, s.length());
}

jl_value_t *j2::FromJavaScriptNull(v8::Local<v8::Value> value) {
  return jl_nothing;
}

jl_value_t *j2::FromJavaScriptNumber(v8::Local<v8::Value> value) {
  return jl_box_float64(value->ToNumber()->Value());
}

jl_value_t *j2::FromJavaScriptValue(v8::Local<v8::Value> value) {
  if (value->IsBoolean()) {
    return FromJavaScriptBoolean(value);
  }

  if (value->IsNumber()) {
    return FromJavaScriptNumber(value);
  }

  if (value->IsString()) {
    return FromJavaScriptString(value);
  }

  if (value->IsNull()) {
    return FromJavaScriptNull(value);
  }

  if (value->IsArray()) {
    return FromJavaScriptArray(value);
  }

  return jl_nothing;
}

v8::Local<v8::Value> j2::FromJuliaBool(v8::Isolate *isolate,
                                       jl_value_t *value) {
  return v8::Boolean::New(isolate, jl_unbox_bool(value));
}

v8::Local<v8::Value> j2::FromJuliaFloat64(v8::Isolate *isolate,
                                          jl_value_t *value) {
  return v8::Number::New(isolate, jl_unbox_float64(value));
}

v8::Local<v8::Value> j2::FromJuliaNothing(v8::Isolate *isolate,
                                          jl_value_t *value) {
  return v8::Null(isolate);
}

v8::Local<v8::Value> j2::FromJuliaString(v8::Isolate *isolate,
                                         jl_value_t *value) {
  return v8::String::NewFromUtf8(isolate, jl_string_data(value));
}

v8::Local<v8::Value> j2::FromJuliaArray(v8::Isolate *isolate,
                                        jl_value_t *value) {
  return NewArrayDescriptor(isolate, value);
}

v8::Local<v8::Value> j2::FromJuliaValue(v8::Isolate *isolate,
                                        jl_value_t *value) {
  if (jl_is_bool(value)) {
    return FromJuliaBool(isolate, value);
  }

  if (jl_is_float64(value)) {
    return FromJuliaFloat64(isolate, value);
  }

  if (jl_is_nothing(value)) {
    return FromJuliaNothing(isolate, value);
  }

  if (jl_is_string(value)) {
    return FromJuliaString(isolate, value);
  }

  if (jl_is_array(value)) {
    return FromJuliaArray(isolate, value);
  }

  return v8::Undefined(isolate);
}
