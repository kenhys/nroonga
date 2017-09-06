#include "nroonga.h"

namespace nroonga {

Nan::Persistent<v8::Function> groonga_context_constructor;

void Database::Initialize(v8::Local<v8::Object> exports) {
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);

  tpl->SetClassName(Nan::New("Database").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  Nan::SetPrototypeMethod(tpl, "commandString", Database::CommandString);
  Nan::SetPrototypeMethod(tpl, "commandSyncString", Database::CommandSyncString);
  Nan::SetPrototypeMethod(tpl, "close", Database::Close);

  groonga_context_constructor.Reset(tpl->GetFunction());
  exports->Set(Nan::New("Database").ToLocalChecked(),
               tpl->GetFunction());
}

void Database::New(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  if (!info.IsConstructCall()) {
    Nan::ThrowTypeError("Use the new operator to create new Database objects");
    return;
  }

  Database *db = new Database();
  db->closed = true;
  grn_ctx *ctx = &db->context;
  grn_ctx_init(ctx, 0);
  if (info[0]->IsUndefined()) {
    db->database = grn_db_create(ctx, NULL, NULL);
  } else if (info[0]->IsString()) {
    v8::String::Utf8Value path(info[0]->ToString());
    if (info.Length() > 1 && info[1]->IsTrue()) {
      db->database = grn_db_open(ctx, *path);
    } else {
      GRN_DB_OPEN_OR_CREATE(ctx, *path, NULL, db->database);
    }
    if (ctx->rc != GRN_SUCCESS) {
      Nan::ThrowTypeError(ctx->errbuf);
      return;
    }
  } else {
    Nan::ThrowTypeError("Bad parameter");
    return;
  }

  db->closed = false;
  db->Wrap(info.Holder());
  info.GetReturnValue().Set(info.This());
}

bool Database::Cleanup() {
  if (grn_obj_close(&context, database) != GRN_SUCCESS) {
    return false;
  }
  if (grn_ctx_fin(&context) != GRN_SUCCESS) {
    return false;
  }
  this->closed = true;

  return true;
}

void Database::Close(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  Database *db = ObjectWrap::Unwrap<Database>(info.Holder());

  if (db->closed) {
    Nan::ThrowTypeError("Database already closed");
    return;
  }

  if (db->Cleanup()) {
    info.GetReturnValue().Set(Nan::True());
    return;
  }
  Nan::ThrowTypeError("Failed to close the database");
}

void Database::CommandWork(uv_work_t* req) {
  Baton *baton = static_cast<Baton*>(req->data);
  int rc = -1;
  int flags;
  grn_ctx *ctx = &baton->context;

  grn_ctx_init(ctx, 0);
  grn_ctx_use(ctx, baton->database);
  rc = grn_ctx_send(ctx, baton->command.c_str(), baton->command.size(), 0);
  if (rc < 0) {
    baton->error = 1;
    return;
  }
  if (ctx->rc != GRN_SUCCESS) {
    baton->error = 2;
    return;
  }

  grn_ctx_recv(ctx, &baton->result, &baton->result_length, &flags);
  if (ctx->rc < 0) {
    baton->error = 3;
    return;
  }
  baton->error = 0;
}

void Database::CommandAfter(uv_work_t* req) {
  Nan::HandleScope scope;

  Baton* baton = static_cast<Baton*>(req->data);
  v8::Handle<v8::Value> argv[2];
  if (baton->error) {
    argv[0] = v8::Exception::Error(
        Nan::New(baton->context.errbuf).ToLocalChecked());
    argv[1] = Nan::Null();
  } else {
    argv[0] = Nan::Null();
    argv[1] = Nan::NewBuffer(baton->result, baton->result_length)
        .ToLocalChecked();
  }
  Nan::New<v8::Function>(baton->callback)
      ->Call(Nan::GetCurrentContext()->Global(), 2, argv);
  grn_ctx_fin(&baton->context);
  delete baton;
}

void Database::CommandString(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  Database *db = ObjectWrap::Unwrap<Database>(info.Holder());
  if (info.Length() < 1 || !info[0]->IsString()) {
    Nan::ThrowTypeError("Bad parameter");
    return;
  }

  v8::Local<v8::Function> callback;
  if (info.Length() >= 2) {
    if (!info[1]->IsFunction()) {
      Nan::ThrowTypeError("Second argument must be a callback function");
      return;
    }
    callback = v8::Local<v8::Function>::Cast(info[1]);
  }

  if (db->closed) {
    Nan::ThrowTypeError("Database already closed");
    return;
  }

  Baton* baton = new Baton();
  baton->request.data = baton;
  baton->callback.Reset(callback);

  v8::String::Utf8Value command(info[0]->ToString());
  baton->database = db->database;

  baton->command = std::string(*command, command.length());
  uv_queue_work(uv_default_loop(),
      &baton->request,
      CommandWork,
      (uv_after_work_cb)CommandAfter
      );

  info.GetReturnValue().Set(Nan::Undefined());
}

void Database::CommandSyncString(
    const Nan::FunctionCallbackInfo<v8::Value>& info) {
  Database *db = ObjectWrap::Unwrap<Database>(info.Holder());
  if (info.Length() < 1 || !info[0]->IsString()) {
    Nan::ThrowTypeError("Bad parameter");
    return;
  }

  int rc = -1;
  grn_ctx *ctx = &db->context;
  char *result;
  unsigned int result_length;
  int flags;
  v8::String::Utf8Value command(info[0]->ToString());

  if (db->closed) {
    Nan::ThrowTypeError("Database already closed");
    return;
  }

  rc = grn_ctx_send(ctx, *command, command.length(), 0);
  if (rc < 0) {
    Nan::ThrowTypeError("grn_ctx_send returned error");
    return;
  }
  if (ctx->rc != GRN_SUCCESS) {
    Nan::ThrowTypeError(ctx->errbuf);
    return;
  }
  grn_ctx_recv(ctx, &result, &result_length, &flags);
  if (ctx->rc < 0) {
    Nan::ThrowTypeError("grn_ctx_recv returned error");
    return;
  }

  info.GetReturnValue().Set(
      Nan::NewBuffer(result, result_length).ToLocalChecked()->ToString());
}

void InitNroonga(v8::Local<v8::Object> exports) {
  grn_init();
  Database::Initialize(exports);
}

} // namespace nroonga

NODE_MODULE(nroonga_bindings, nroonga::InitNroonga);
