// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "frame/object.h"

#include <string>

#include "base/logging.h"
#include "frame/store.h"
#include "string/text.h"

namespace sling {

void Names::Add(Name *name) {
  CHECK(name->next_ == nullptr);
  name->next_ = list_;
  list_ = name;
}

bool Names::Bind(Store *store) {
  bool resolved = true;
  for (Name *n = list_; n != nullptr; n = n->next_) {
    Handle h = store->Lookup(n->name());
    if (h.IsNil()) {
      resolved = false;
    } else {
      n->set_handle(h);
      n->set_store(store);
    }
  }
  return resolved;
}

bool Names::Bind(const Store *store) {
  bool resolved = true;
  for (Name *n = list_; n != nullptr; n = n->next_) {
    Handle h = store->LookupExisting(n->name());
    if (h.IsNil()) {
      resolved = false;
    } else {
      n->set_handle(h);
      n->set_store(store);
    }
  }
  return resolved;
}

Object &Object::operator =(const Object &other) {
  Unlink();
  handle_ = other.handle_;
  store_ = other.store_;
  if (other.locked()) Link(&other);
  return *this;
}

Type Object::type() const {
  if (handle_.IsRef()) {
    return datum()->type();
  } else {
    return static_cast<Type>(handle_.tag() | Handle::kSimple);
  }
}

String Object::AsString() const {
  return String(store(), store()->Cast(handle(), STRING));
}

Frame Object::AsFrame() const {
  return Frame(store(), store()->Cast(handle(), FRAME));
}

Symbol Object::AsSymbol() const {
  return Symbol(store(), store()->Cast(handle(), SYMBOL));
}

Array Object::AsArray() const {
  return Array(store(), store()->Cast(handle(), ARRAY));
}

String::String(Store *store, Handle handle) : Object(store, handle) {
  DCHECK(IsNil() || IsString());
}

String::String(Store *store, Text str)
    : Object(store, store->AllocateString(str)) {}

String &String::operator =(const String &other) {
  Unlink();
  handle_ = other.handle_;
  store_ = other.store_;
  if (other.locked()) Link(&other);
  return *this;
}

Frame::Frame(Store *store, Handle handle) : Object(store, handle) {
  DCHECK(IsNil() || IsFrame());
}

Frame::Frame(Store *store, Text id) : Object(store, id) {
  DCHECK(IsNil() || IsFrame());
}

Frame::Frame(Store *store, Slot *begin, Slot *end)
    : Object(store, store->AllocateFrame(begin, end)) {
  DCHECK(IsNil() || IsFrame()) << Type();
}

string Frame::Id() const {
  if (IsNil()) return "";
  Handle id = frame()->get(Handle::id());
  if (id.IsNil()) return "";
  Datum *datum = store_->Deref(id);
  if (!datum->IsSymbol()) return "";
  SymbolDatum *symbol = datum->AsSymbol();
  if (symbol->numeric()) {
    return StrCat("#", symbol->name.AsInt());
  } else {
    StringDatum *symstr = store_->GetString(symbol->name);
    return string(symstr->data(), symstr->size());
  }
}

Text Frame::IdStr() const {
  if (IsNil()) return Text();
  Handle id = frame()->get(Handle::id());
  if (id.IsNil()) return Text();
  Datum *datum = store_->Deref(id);
  if (!datum->IsSymbol()) return Text();
  SymbolDatum *symbol = datum->AsSymbol();
  if (symbol->numeric()) return Text();
  StringDatum *symstr = store_->GetString(symbol->name);
  return symstr->str();
}

Frame &Frame::operator =(const Frame &other) {
  Unlink();
  handle_ = other.handle_;
  store_ = other.store_;
  if (other.locked()) Link(&other);
  return *this;
}

bool Frame::Has(Handle name) const {
  return frame()->has(name);
}

bool Frame::Has(const Object &name) const {
  return Has(name.handle());
}

bool Frame::Has(const Name &name) const {
  return Has(name.Lookup(store_));
}

bool Frame::Has(Text name) const {
  return Has(store()->Lookup(name));
}

Object Frame::Get(Handle name) const {
  return Object(store(), frame()->get(name));
}

Object Frame::Get(const Object &name) const {
  return Get(name.handle());
}

Object Frame::Get(const Name &name) const {
  return Get(name.Lookup(store_));
}

Object Frame::Get(Text name) const {
  return Get(store()->Lookup(name));
}

Frame Frame::GetFrame(Handle name) const {
  Handle value = frame()->get(name);
  return Frame(store(), store()->Cast(value, FRAME));
}

Frame Frame::GetFrame(const Object &name) const {
  return GetFrame(name.handle());
}

Frame Frame::GetFrame(const Name &name) const {
  return GetFrame(name.Lookup(store_));
}

Frame Frame::GetFrame(Text name) const {
  return GetFrame(store()->Lookup(name));
}

Symbol Frame::GetSymbol(Handle name) const {
  Handle value = frame()->get(name);
  return Symbol(store(), store()->Cast(value, SYMBOL));
}

Symbol Frame::GetSymbol(const Object &name) const {
  return GetSymbol(name.handle());
}

Symbol Frame::GetSymbol(const Name &name) const {
  return GetSymbol(name.Lookup(store_));
}

Symbol Frame::GetSymbol(Text name) const {
  return GetSymbol(store()->Lookup(name));
}

string Frame::GetString(Handle name) const {
  Handle value = frame()->get(name);
  if (value.IsRef() && !value.IsNil()) {
    Datum *datum = store()->Deref(value);
    if (datum->IsString()) return datum->AsString()->str().ToString();
  }
  return "";
}

string Frame::GetString(const Object &name) const {
  return GetString(name.handle());
}

string Frame::GetString(const Name &name) const {
  return GetString(name.Lookup(store_));
}

string Frame::GetString(Text name) const {
  return GetString(store()->Lookup(name));
}

Text Frame::GetText(Handle name) const {
  Handle value = frame()->get(name);
  if (value.IsRef() && !value.IsNil()) {
    Datum *datum = store()->Deref(value);
    if (datum->IsString()) return datum->AsString()->str();
  }
  return Text();
}

Text Frame::GetText(const Object &name) const {
  return GetText(name.handle());
}

Text Frame::GetText(const Name &name) const {
  return GetText(name.Lookup(store_));
}

Text Frame::GetText(Text name) const {
  return GetText(store()->Lookup(name));
}

int Frame::GetInt(Handle name, int defval) const {
  Handle value = frame()->get(name);
  return value.IsInt() ? value.AsInt() : defval;
}

int Frame::GetInt(const Object &name, int defval) const {
  return GetInt(name.handle(), defval);
}

int Frame::GetInt(const Name &name, int defval) const {
  return GetInt(name.Lookup(store_), defval);
}

int Frame::GetInt(Text name, int defval) const {
  return GetInt(store()->Lookup(name), defval);
}

bool Frame::GetBool(Handle name) const {
  Handle value = frame()->get(name);
  return value.IsInt() ? value.IsTrue() : false;
}

bool Frame::GetBool(const Object &name) const {
  return GetBool(name.handle());
}

bool Frame::GetBool(const Name &name) const {
  return GetBool(name.Lookup(store_));
}

bool Frame::GetBool(Text name) const {
  return GetBool(store()->Lookup(name));
}

float Frame::GetFloat(Handle name) const {
  Handle value = frame()->get(name);
  return value.IsFloat() ? value.AsFloat() : 0.0;
}

float Frame::GetFloat(const Object &name) const {
  return GetFloat(name.handle());
}

float Frame::GetFloat(const Name &name) const {
  return GetFloat(name.Lookup(store_));
}

float Frame::GetFloat(Text name) const {
  return GetFloat(store()->Lookup(name));
}

Handle Frame::GetHandle(Handle name) const {
  return frame()->get(name);
}

Handle Frame::GetHandle(const Object &name) const {
  return GetHandle(name.handle());
}

Handle Frame::GetHandle(const Name &name) const {
  return GetHandle(name.Lookup(store_));
}

Handle Frame::GetHandle(Text name) const {
  return GetHandle(store()->Lookup(name));
}

bool Frame::IsA(Handle type) const {
  for (const Slot *slot = frame()->begin(); slot < frame()->end(); ++slot) {
    if (slot->name.IsIsA() && slot->value == type) return true;
  }
  return false;
}

bool Frame::IsA(const Name &type) const {
  return IsA(type.Lookup(store_));
}

bool Frame::Is(Handle type) const {
  for (const Slot *slot = frame()->begin(); slot < frame()->end(); ++slot) {
    if (slot->name.IsIs() && slot->value == type) return true;
  }
  return false;
}

bool Frame::Is(const Name &type) const {
  return Is(type.Lookup(store_));
}

void Frame::Add(Handle name, Handle value) {
  store_->Add(handle_, name, value);
}

void Frame::Add(const Object &name, Handle value) {
  store_->Add(handle_, name.handle(), value);
}

void Frame::Add(const Name &name, Handle value) {
  store_->Add(handle_, name.Lookup(store_), value);
}

void Frame::Add(Text name, Handle value) {
  store_->Add(handle_, store_->Lookup(name), value);
}

void Frame::Add(Handle value) {
  store_->Add(handle_, Handle::nil(), value);
}

void Frame::Add(Handle name, const Object &value) {
  store_->Add(handle_, name, value.handle());
}

void Frame::Add(Handle name, const Name &value) {
  store_->Add(handle_, name, value.Lookup(store_));
}

void Frame::Add(const Object &name, const Object &value) {
  store_->Add(handle_, name.handle(), value.handle());
}

void Frame::Add(const Object &name, const Name &value) {
  store_->Add(handle_, name.handle(), value.Lookup(store_));
}

void Frame::Add(const Name &name, const Object &value) {
  store_->Add(handle_, name.Lookup(store_), value.handle());
}

void Frame::Add(const Name &name, const Name &value) {
  store_->Add(handle_, name.Lookup(store_), value.Lookup(store_));
}

void Frame::Add(Text name, const Object &value) {
  store_->Add(handle_, store_->Lookup(name), value.handle());
}

void Frame::Add(Text name, const Name &value) {
  store_->Add(handle_, store_->Lookup(name), value.Lookup(store_));
}

void Frame::Add(Handle name, int value) {
  store_->Add(handle_, name, Handle::Integer(value));
}

void Frame::Add(const Object &name, int value) {
  store_->Add(handle_, name.handle(), Handle::Integer(value));
}

void Frame::Add(const Name &name, int value) {
  store_->Add(handle_, name.Lookup(store_), Handle::Integer(value));
}

void Frame::Add(Text name, int value) {
  store_->Add(handle_, store_->Lookup(name), Handle::Integer(value));
}

void Frame::Add(int value) {
  store_->Add(handle_, Handle::nil(), Handle::Integer(value));
}

void Frame::Add(Handle name, bool value) {
  store_->Add(handle_, name, Handle::Bool(value));
}

void Frame::Add(const Object &name, bool value) {
  store_->Add(handle_, name.handle(), Handle::Bool(value));
}

void Frame::Add(const Name &name, bool value) {
  store_->Add(handle_, name.Lookup(store_), Handle::Bool(value));
}

void Frame::Add(Text name, bool value) {
  store_->Add(handle_, store_->Lookup(name), Handle::Bool(value));
}

void Frame::Add(bool value) {
  store_->Add(handle_, Handle::nil(), Handle::Bool(value));
}

void Frame::Add(Handle name, float value) {
  store_->Add(handle_, name, Handle::Float(value));
}

void Frame::Add(const Object &name, float value) {
  store_->Add(handle_, name.handle(), Handle::Float(value));
}

void Frame::Add(const Name &name, float value) {
  store_->Add(handle_, name.Lookup(store_), Handle::Float(value));
}

void Frame::Add(Text name, float value) {
  store_->Add(handle_, store_->Lookup(name), Handle::Float(value));
}

void Frame::Add(float value) {
  store_->Add(handle_, Handle::nil(), Handle::Float(value));
}

void Frame::Add(Handle name, double value) {
  store_->Add(handle_, name, Handle::Float(value));
}

void Frame::Add(const Object &name, double value) {
  store_->Add(handle_, name.handle(), Handle::Float(value));
}

void Frame::Add(const Name &name, double value) {
  store_->Add(handle_, name.Lookup(store_), Handle::Float(value));
}

void Frame::Add(Text name, double value) {
  store_->Add(handle_, store_->Lookup(name), Handle::Float(value));
}

void Frame::Add(double value) {
  store_->Add(handle_, Handle::nil(), Handle::Float(value));
}

void Frame::Add(Handle name, Text value) {
  store_->LockGC();
  store_->Add(handle_, name, store_->AllocateString(value));
  store_->UnlockGC();
}

void Frame::Add(const Object &name, Text value) {
  store_->LockGC();
  store_->Add(handle_, name.handle(), store_->AllocateString(value));
  store_->UnlockGC();
}

void Frame::Add(const Name &name, Text value) {
  store_->LockGC();
  store_->Add(handle_, name.Lookup(store_), store_->AllocateString(value));
  store_->UnlockGC();
}

void Frame::Add(Text name, Text value) {
  store_->LockGC();
  store_->Add(handle_, store_->Lookup(name), store_->AllocateString(value));
  store_->UnlockGC();
}

void Frame::Add(Text value) {
  store_->LockGC();
  store_->Add(handle_, Handle::nil(), store_->AllocateString(value));
  store_->UnlockGC();
}

void Frame::Add(Handle name, const char *value) {
  store_->LockGC();
  store_->Add(handle_, name, store_->AllocateString(value));
  store_->UnlockGC();
}

void Frame::Add(const Object &name, const char *value) {
  store_->LockGC();
  store_->Add(handle_, name.handle(), store_->AllocateString(value));
  store_->UnlockGC();
}

void Frame::Add(const Name &name, const char *value) {
  store_->LockGC();
  store_->Add(handle_, name.Lookup(store_), store_->AllocateString(value));
  store_->UnlockGC();
}

void Frame::Add(Text name, const char *value) {
  store_->LockGC();
  store_->Add(handle_, store_->Lookup(name), store_->AllocateString(value));
  store_->UnlockGC();
}

void Frame::Add(const char *value) {
  store_->LockGC();
  store_->Add(handle_, Handle::nil(), store_->AllocateString(value));
  store_->UnlockGC();
}

void Frame::AddLink(Handle name, Text symbol) {
  store_->Add(handle_, name, store_->Lookup(symbol));
}

void Frame::AddLink(const Object &name, Text symbol) {
  store_->Add(handle_, name.handle(), store_->Lookup(symbol));
}

void Frame::AddLink(const Name &name, Text symbol) {
  store_->Add(handle_, name.Lookup(store_), store_->Lookup(symbol));
}

void Frame::AddLink(Text name, Text symbol) {
  store_->Add(handle_, store_->Lookup(name), store_->Lookup(symbol));
}

void Frame::AddLink(Text symbol) {
  store_->Add(handle_, Handle::nil(), store_->Lookup(symbol));
}

void Frame::AddIsA(Handle type) {
  store_->Add(handle_, Handle::isa(), type);
}

void Frame::AddIsA(const Object &type) {
  store_->Add(handle_, Handle::isa(), type.handle());
}

void Frame::AddIsA(const Name &type) {
  store_->Add(handle_, Handle::isa(), type.Lookup(store_));
}

void Frame::AddIsA(Text type) {
  store_->Add(handle_, Handle::isa(), store_->Lookup(type));
}

void Frame::AddIsA(const String &type) {
  store_->Add(handle_, Handle::isa(), store_->Lookup(type.handle()));
}

void Frame::AddIs(Handle type) {
  store_->Add(handle_, Handle::is(), type);
}

void Frame::AddIs(const Object &type) {
  store_->Add(handle_, Handle::is(), type.handle());
}

void Frame::AddIs(const Name &type) {
  store_->Add(handle_, Handle::is(), type.Lookup(store_));
}

void Frame::AddIs(Text type) {
  store_->Add(handle_, Handle::is(), store_->Lookup(type));
}

void Frame::AddIs(const String &type) {
  store_->Add(handle_, Handle::is(), store_->Lookup(type.handle()));
}

void Frame::Set(Handle name, Handle value) {
  store_->Set(handle_, name, value);
}

void Frame::Set(const Object &name, Handle value) {
  store_->Set(handle_, name.handle(), value);
}

void Frame::Set(const Name &name, Handle value) {
  store_->Set(handle_, name.Lookup(store_), value);
}

void Frame::Set(Text name, Handle value) {
  store_->Set(handle_, store_->Lookup(name), value);
}

void Frame::Set(Handle name, const Object &value) {
  store_->Set(handle_, name, value.handle());
}

void Frame::Set(Handle name, const Name &value) {
  store_->Set(handle_, name, value.Lookup(store_));
}

void Frame::Set(const Object &name, const Object &value) {
  store_->Set(handle_, name.handle(), value.handle());
}

void Frame::Set(const Object &name, const Name &value) {
  store_->Set(handle_, name.handle(), value.Lookup(store_));
}

void Frame::Set(const Name &name, const Object &value) {
  store_->Set(handle_, name.Lookup(store_), value.handle());
}

void Frame::Set(const Name &name, const Name &value) {
  store_->Set(handle_, name.Lookup(store_), value.Lookup(store_));
}

void Frame::Set(Text name, const Object &value) {
  store_->Set(handle_, store_->Lookup(name), value.handle());
}

void Frame::Set(Text name, const Name &value) {
  store_->Set(handle_, store_->Lookup(name), value.Lookup(store_));
}

void Frame::Set(Handle name, int value) {
  store_->Set(handle_, name,  Handle::Integer(value));
}

void Frame::Set(const Object &name, int value) {
  store_->Set(handle_, name.handle(),  Handle::Integer(value));
}

void Frame::Set(const Name &name, int value) {
  store_->Set(handle_, name.Lookup(store_),  Handle::Integer(value));
}

void Frame::Set(Text name, int value) {
  store_->Set(handle_, store_->Lookup(name),  Handle::Integer(value));
}

void Frame::Set(Handle name, bool value) {
  store_->Set(handle_, name,  Handle::Bool(value));
}

void Frame::Set(const Object &name, bool value) {
  store_->Set(handle_, name.handle(),  Handle::Bool(value));
}

void Frame::Set(const Name &name, bool value) {
  store_->Set(handle_, name.Lookup(store_),  Handle::Bool(value));
}

void Frame::Set(Text name, bool value) {
  store_->Set(handle_, store_->Lookup(name),  Handle::Bool(value));
}

void Frame::Set(Handle name, float value) {
  store_->Set(handle_, name,  Handle::Float(value));
}

void Frame::Set(const Object &name, float value) {
  store_->Set(handle_, name.handle(),  Handle::Float(value));
}

void Frame::Set(const Name &name, float value) {
  store_->Set(handle_, name.Lookup(store_),  Handle::Float(value));
}

void Frame::Set(Text name, float value) {
  store_->Set(handle_, store_->Lookup(name),  Handle::Float(value));
}

void Frame::Set(Handle name, double value) {
  store_->Set(handle_, name,  Handle::Float(value));
}

void Frame::Set(const Object &name, double value) {
  store_->Set(handle_, name.handle(),  Handle::Float(value));
}

void Frame::Set(const Name &name, double value) {
  store_->Set(handle_, name.Lookup(store_),  Handle::Float(value));
}

void Frame::Set(Text name, double value) {
  store_->Set(handle_, store_->Lookup(name),  Handle::Float(value));
}

void Frame::Set(Handle name, Text value) {
  store_->LockGC();
  store_->Set(handle_, name, store_->AllocateString(value));
  store_->UnlockGC();
}

void Frame::Set(const Object &name, Text value) {
  store_->LockGC();
  store_->Set(handle_, name.handle(), store_->AllocateString(value));
  store_->UnlockGC();
}

void Frame::Set(const Name &name, Text value) {
  store_->LockGC();
  store_->Set(handle_, name.Lookup(store_), store_->AllocateString(value));
  store_->UnlockGC();
}

void Frame::Set(Text name, Text value) {
  store_->LockGC();
  store_->Set(handle_, store_->Lookup(name), store_->AllocateString(value));
  store_->UnlockGC();
}

void Frame::Set(Handle name, const char *value) {
  store_->LockGC();
  store_->Set(handle_, name, store_->AllocateString(value));
  store_->UnlockGC();
}

void Frame::Set(const Object &name, const char *value) {
  store_->LockGC();
  store_->Set(handle_, name.handle(), store_->AllocateString(value));
  store_->UnlockGC();
}

void Frame::Set(const Name &name, const char *value) {
  store_->LockGC();
  store_->Set(handle_, name.Lookup(store_), store_->AllocateString(value));
  store_->UnlockGC();
}

void Frame::Set(Text name, const char *value) {
  store_->LockGC();
  store_->Set(handle_, store_->Lookup(name), store_->AllocateString(value));
  store_->UnlockGC();
}

void Frame::SetLink(Handle name, Text symbol) {
  store_->Set(handle_, name, store_->Lookup(symbol));
}

void Frame::SetLink(const Object &name, Text symbol) {
  store_->Set(handle_, name.handle(), store_->Lookup(symbol));
}

void Frame::SetLink(const Name &name, Text symbol) {
  store_->Set(handle_, name.Lookup(store_), store_->Lookup(symbol));
}

void Frame::SetLink(Text name, Text symbol) {
  store_->Set(handle_, store_->Lookup(name), store_->Lookup(symbol));
}

Symbol::Symbol(Store *store, Handle handle) : Object(store, handle) {
  DCHECK(IsNil() || IsSymbol()) << Type();
}

Symbol::Symbol(Store *store, Text id) : Object(store, store->Symbol(id)) {
  DCHECK(IsNil() || IsSymbol()) << Type();
}

Symbol &Symbol::operator =(const Symbol &other) {
  Unlink();
  handle_ = other.handle_;
  store_ = other.store_;
  if (other.locked()) Link(&other);
  return *this;
}

Object Symbol::GetName() const {
  return Object(store(), symbol()->name);
}

Object Symbol::GetValue() const {
  return Object(store(), symbol()->value);
}

Array::Array(Store *store, Handle handle) : Object(store, handle) {
  DCHECK(IsNil() || IsArray()) << Type();
}

Array::Array(Store *store, int size)
    : Object(store, store->AllocateArray(size)) {
  DCHECK(IsNil() || IsArray()) << Type();
}

Array::Array(Store *store, const Handle *begin, const Handle *end)
    : Object(store, store->AllocateArray(begin, end)) {
  DCHECK(IsNil() || IsArray()) << Type();
}

Array &Array::operator =(const Array &other) {
  Unlink();
  handle_ = other.handle_;
  store_ = other.store_;
  if (other.locked()) Link(&other);
  return *this;
}

Builder::Builder(Store *store) : External(store), store_(store) {
  handle_ = Handle::nil();
  slots_.reserve(kInitialSlots * sizeof(Slot));
}

Builder::Builder(const Frame &frame)
    : External(frame.store()),
      store_(frame.store()) {
  handle_ = frame.handle();
  AddFrom(handle_);
}

Builder::Builder(Store *store, Handle handle)
    : External(store),
      store_(store),
      handle_(handle) {
  AddFrom(handle);
}

Builder::Builder(Store *store, Text id) : External(store), store_(store) {
  handle_ = store->Lookup(id);
  AddFrom(handle_);
}

Builder::~Builder() {
}

void Builder::Add(Handle name, Handle value) {
  Slot *slot = NewSlot();
  slot->name = name;
  slot->value = value;
}

void Builder::Add(const Object &name, Handle value) {
  Slot *slot = NewSlot();
  slot->name = name.handle();
  slot->value = value;
}

void Builder::Add(const Name &name, Handle value) {
  Slot *slot = NewSlot();
  slot->name = name.Lookup(store_);
  slot->value = value;
}

void Builder::Add(Text name, Handle value) {
  Slot *slot = NewSlot();
  slot->name = store_->Lookup(name);
  slot->value = value;
}

void Builder::Add(Handle value) {
  Slot *slot = NewSlot();
  slot->name = Handle::nil();
  slot->value = value;
}

void Builder::Add(Handle name, const Object &value) {
  Slot *slot = NewSlot();
  slot->name = name;
  slot->value = value.handle();
}

void Builder::Add(Handle name, const Name &value) {
  Slot *slot = NewSlot();
  slot->name = name;
  slot->value = value.Lookup(store_);
}

void Builder::Add(const Object &name, const Object &value) {
  Slot *slot = NewSlot();
  slot->name = name.handle();
  slot->value = value.handle();
}

void Builder::Add(const Object &name, const Name &value) {
  Slot *slot = NewSlot();
  slot->name = name.handle();
  slot->value = value.Lookup(store_);
}

void Builder::Add(const Name &name, const Object &value) {
  Slot *slot = NewSlot();
  slot->name = name.Lookup(store_);
  slot->value = value.handle();
}

void Builder::Add(const Name &name, const Name &value) {
  Slot *slot = NewSlot();
  slot->name = name.Lookup(store_);
  slot->value = value.Lookup(store_);
}

void Builder::Add(Text name, const Object &value) {
  Slot *slot = NewSlot();
  slot->name = store_->Lookup(name);
  slot->value = value.handle();
}

void Builder::Add(Text name, const Name &value) {
  Slot *slot = NewSlot();
  slot->name = store_->Lookup(name);
  slot->value = value.Lookup(store_);
}

void Builder::Add(Handle name, int value) {
  Slot *slot = NewSlot();
  slot->name = name;
  slot->value = Handle::Integer(value);
}

void Builder::Add(const Object &name, int value) {
  Slot *slot = NewSlot();
  slot->name = name.handle();
  slot->value = Handle::Integer(value);
}

void Builder::Add(const Name &name, int value) {
  Slot *slot = NewSlot();
  slot->name = name.Lookup(store_);
  slot->value = Handle::Integer(value);
}

void Builder::Add(Text name, int value) {
  Slot *slot = NewSlot();
  slot->name = store_->Lookup(name);
  slot->value = Handle::Integer(value);
}

void Builder::Add(int value) {
  Slot *slot = NewSlot();
  slot->name = Handle::nil();
  slot->value = Handle::Integer(value);
}

void Builder::Add(Handle name, bool value) {
  Slot *slot = NewSlot();
  slot->name = name;
  slot->value = Handle::Bool(value);
}

void Builder::Add(const Object &name, bool value) {
  Slot *slot = NewSlot();
  slot->name = name.handle();
  slot->value = Handle::Bool(value);
}

void Builder::Add(const Name &name, bool value) {
  Slot *slot = NewSlot();
  slot->name = name.Lookup(store_);
  slot->value = Handle::Bool(value);
}

void Builder::Add(Text name, bool value) {
  Slot *slot = NewSlot();
  slot->name = store_->Lookup(name);
  slot->value = Handle::Bool(value);
}

void Builder::Add(bool value) {
  Slot *slot = NewSlot();
  slot->name = Handle::nil();
  slot->value = Handle::Bool(value);
}

void Builder::Add(Handle name, float value) {
  Slot *slot = NewSlot();
  slot->name = name;
  slot->value = Handle::Float(value);
}

void Builder::Add(const Object &name, float value) {
  Slot *slot = NewSlot();
  slot->name = name.handle();
  slot->value = Handle::Float(value);
}

void Builder::Add(const Name &name, float value) {
  Slot *slot = NewSlot();
  slot->name = name.Lookup(store_);
  slot->value = Handle::Float(value);
}

void Builder::Add(Text name, float value) {
  Slot *slot = NewSlot();
  slot->name = store_->Lookup(name);
  slot->value = Handle::Float(value);
}

void Builder::Add(float value) {
  Slot *slot = NewSlot();
  slot->name = Handle::nil();
  slot->value = Handle::Float(value);
}

void Builder::Add(Handle name, double value) {
  Slot *slot = NewSlot();
  slot->name = name;
  slot->value = Handle::Float(value);
}

void Builder::Add(const Object &name, double value) {
  Slot *slot = NewSlot();
  slot->name = name.handle();
  slot->value = Handle::Float(value);
}

void Builder::Add(const Name &name, double value) {
  Slot *slot = NewSlot();
  slot->name = name.Lookup(store_);
  slot->value = Handle::Float(value);
}

void Builder::Add(Text name, double value) {
  Slot *slot = NewSlot();
  slot->name = store_->Lookup(name);
  slot->value = Handle::Float(value);
}

void Builder::Add(double value) {
  Slot *slot = NewSlot();
  slot->name = Handle::nil();
  slot->value = Handle::Float(value);
}

void Builder::Add(Handle name, Text value) {
  Slot *slot = NewSlot();
  slot->name = name;
  slot->value = store_->AllocateString(value);
}

void Builder::Add(const Object &name, Text value) {
  Slot *slot = NewSlot();
  slot->name = name.handle();
  slot->value = store_->AllocateString(value);
}

void Builder::Add(const Name &name, Text value) {
  Slot *slot = NewSlot();
  slot->name = name.Lookup(store_);
  slot->value = store_->AllocateString(value);
}

void Builder::Add(Text name, Text value) {
  Slot *slot = NewSlot();
  slot->name = store_->Lookup(name);
  slot->value = store_->AllocateString(value);
}

void Builder::Add(Text value) {
  Slot *slot = NewSlot();
  slot->name = Handle::nil();
  slot->value = store_->AllocateString(value);
}

void Builder::Add(Handle name, const char *value) {
  Slot *slot = NewSlot();
  slot->name = name;
  slot->value = store_->AllocateString(value);
}

void Builder::Add(const Object &name, const char *value) {
  Slot *slot = NewSlot();
  slot->name = name.handle();
  slot->value = store_->AllocateString(value);
}

void Builder::Add(const Name &name, const char *value) {
  Slot *slot = NewSlot();
  slot->name = name.Lookup(store_);
  slot->value = store_->AllocateString(value);
}

void Builder::Add(Text name, const char *value) {
  Slot *slot = NewSlot();
  slot->name = store_->Lookup(name);
  slot->value = store_->AllocateString(value);
}

void Builder::Add(const char *value) {
  Slot *slot = NewSlot();
  slot->name = Handle::nil();
  slot->value = store_->AllocateString(value);
}

void Builder::AddLink(Handle name, Text symbol) {
  Slot *slot = NewSlot();
  slot->name = name;
  slot->value = store_->Lookup(symbol);
}

void Builder::AddLink(const Object &name, Text symbol) {
  Slot *slot = NewSlot();
  slot->name = name.handle();
  slot->value = store_->Lookup(symbol);
}

void Builder::AddLink(const Name &name, Text symbol) {
  Slot *slot = NewSlot();
  slot->name = name.Lookup(store_);
  slot->value = store_->Lookup(symbol);
}

void Builder::AddLink(Text name, Text symbol) {
  Slot *slot = NewSlot();
  slot->name = store_->Lookup(name);
  slot->value = store_->Lookup(symbol);
}

void Builder::AddLink(Text symbol) {
  Slot *slot = NewSlot();
  slot->name = Handle::nil();
  slot->value = store_->Lookup(symbol);
}

Handle Builder::Builder::AddId() {
  Slot *slot = NewSlot();
  slot->name = Handle::id();
  slot->value = store_->Symbol();
  return slot->value;
}

void Builder::Builder::AddId(Handle id) {
  Slot *slot = NewSlot();
  slot->name = Handle::id();
  slot->value = id;
}

void Builder::Builder::AddId(const Object &id) {
  Slot *slot = NewSlot();
  slot->name = Handle::id();
  slot->value = id.handle();
}

void Builder::AddId(Text id) {
  Slot *slot = NewSlot();
  slot->name = Handle::id();
  slot->value = store_->Symbol(id);
}

void Builder::AddId(const String &id) {
  Slot *slot = NewSlot();
  slot->name = Handle::id();
  slot->value = store_->Symbol(id.handle());
}

void Builder::AddIsA(Handle type) {
  Slot *slot = NewSlot();
  slot->name = Handle::isa();
  slot->value = type;
}

void Builder::AddIsA(const Object &type) {
  Slot *slot = NewSlot();
  slot->name = Handle::isa();
  slot->value = type.handle();
}

void Builder::AddIsA(const Name &type) {
  Slot *slot = NewSlot();
  slot->name = Handle::isa();
  slot->value = type.Lookup(store_);
}

void Builder::AddIsA(Text type) {
  Slot *slot = NewSlot();
  slot->name = Handle::isa();
  slot->value = store_->Lookup(type);
}

void Builder::AddIsA(const String &type) {
  Slot *slot = NewSlot();
  slot->name = Handle::isa();
  slot->value = store_->Lookup(type.handle());
}

void Builder::AddIs(Handle type) {
  Slot *slot = NewSlot();
  slot->name = Handle::is();
  slot->value = type;
}

void Builder::AddIs(const Object &type) {
  Slot *slot = NewSlot();
  slot->name = Handle::is();
  slot->value = type.handle();
}

void Builder::AddIs(const Name &type) {
  Slot *slot = NewSlot();
  slot->name = Handle::is();
  slot->value = type.Lookup(store_);
}

void Builder::AddIs(Text type) {
  Slot *slot = NewSlot();
  slot->name = Handle::is();
  slot->value = store_->Lookup(type);
}

void Builder::AddIs(const String &type) {
  Slot *slot = NewSlot();
  slot->name = Handle::is();
  slot->value = store_->Lookup(type.handle());
}

void Builder::AddFrom(Handle other) {
  FrameDatum *frame = store_->GetFrame(other);
  memcpy(slots_.alloc(frame->size()), frame->begin(), frame->size());
}

void Builder::Delete(Handle name) {
  Slot *slot = slots_.base();
  Slot *end = slots_.end();
  while (slot < end && slot->name != name) slot++;
  if (slot == end) return;
  Slot *current = slot;
  while (slot < end) {
    if (slot->name == name) {
      slot++;
    } else {
      *current++ = *slot++;
    }
  }
  slots_.set_end(current);
}

void Builder::Delete(const Object &name) {
  Delete(name.handle());
}

void Builder::Delete(const Name &name) {
  Delete(name.Lookup(store_));
}

void Builder::Delete(Text name) {
  Delete(store_->Lookup(name));
}

void Builder::Set(Handle name, Handle value) {
  NamedSlot(name)->value = value;
}

void Builder::Set(const Object &name, Handle value) {
  NamedSlot(name.handle())->value = value;
}

void Builder::Set(const Name &name, Handle value) {
  NamedSlot(name.Lookup(store_))->value = value;
}

void Builder::Set(Text name, Handle value) {
  NamedSlot(store_->Lookup(name))->value = value;
}

void Builder::Set(Handle name, const Object &value) {
  NamedSlot(name)->value = value.handle();
}

void Builder::Set(Handle name, const Name &value) {
  NamedSlot(name)->value = value.Lookup(store_);
}

void Builder::Set(const Object &name, const Object &value) {
  NamedSlot(name.handle())->value = value.handle();
}

void Builder::Set(const Object &name, const Name &value) {
  NamedSlot(name.handle())->value = value.Lookup(store_);
}

void Builder::Set(const Name &name, const Object &value) {
  NamedSlot(name.Lookup(store_))->value = value.handle();
}

void Builder::Set(const Name &name, const Name &value) {
  NamedSlot(name.Lookup(store_))->value = value.Lookup(store_);
}

void Builder::Set(Text name, const Object &value) {
  NamedSlot(store_->Lookup(name))->value = value.handle();
}

void Builder::Set(Text name, const Name &value) {
  NamedSlot(store_->Lookup(name))->value = value.Lookup(store_);
}

void Builder::Set(Handle name, int value) {
  NamedSlot(name)->value = Handle::Integer(value);
}

void Builder::Set(const Object &name, int value) {
  NamedSlot(name.handle())->value = Handle::Integer(value);
}

void Builder::Set(const Name &name, int value) {
  NamedSlot(name.Lookup(store_))->value = Handle::Integer(value);
}

void Builder::Set(Text name, int value) {
  NamedSlot(store_->Lookup(name))->value = Handle::Integer(value);
}

void Builder::Set(Handle name, bool value) {
  NamedSlot(name)->value = Handle::Bool(value);
}

void Builder::Set(const Object &name, bool value) {
  NamedSlot(name.handle())->value = Handle::Bool(value);
}

void Builder::Set(const Name &name, bool value) {
  NamedSlot(name.Lookup(store_))->value = Handle::Bool(value);
}

void Builder::Set(Text name, bool value) {
  NamedSlot(store_->Lookup(name))->value = Handle::Bool(value);
}

void Builder::Set(Handle name, float value) {
  NamedSlot(name)->value = Handle::Float(value);
}

void Builder::Set(const Object &name, float value) {
  NamedSlot(name.handle())->value = Handle::Float(value);
}

void Builder::Set(const Name &name, float value) {
  NamedSlot(name.Lookup(store_))->value = Handle::Float(value);
}

void Builder::Set(Text name, float value) {
  NamedSlot(store_->Lookup(name))->value = Handle::Float(value);
}

void Builder::Set(Handle name, double value) {
  NamedSlot(name)->value = Handle::Float(value);
}

void Builder::Set(const Object &name, double value) {
  NamedSlot(name.handle())->value = Handle::Float(value);
}

void Builder::Set(const Name &name, double value) {
  NamedSlot(name.Lookup(store_))->value = Handle::Float(value);
}

void Builder::Set(Text name, double value) {
  NamedSlot(store_->Lookup(name))->value = Handle::Float(value);
}

void Builder::Set(Handle name, Text value) {
  NamedSlot(name)->value = store_->AllocateString(value);
}

void Builder::Set(const Object &name, Text value) {
  NamedSlot(name.handle())->value = store_->AllocateString(value);
}

void Builder::Set(const Name &name, Text value) {
  NamedSlot(name.Lookup(store_))->value = store_->AllocateString(value);
}

void Builder::Set(Text name, Text value) {
  NamedSlot(store_->Lookup(name))->value = store_->AllocateString(value);
}

void Builder::Set(Handle name, const char *value) {
  NamedSlot(name)->value = store_->AllocateString(value);
}

void Builder::Set(const Object &name, const char *value) {
  NamedSlot(name.handle())->value = store_->AllocateString(value);
}

void Builder::Set(const Name &name, const char *value) {
  NamedSlot(name.Lookup(store_))->value = store_->AllocateString(value);
}

void Builder::Set(Text name, const char *value) {
  NamedSlot(store_->Lookup(name))->value = store_->AllocateString(value);
}

void Builder::SetLink(Handle name, Text symbol) {
  NamedSlot(name)->value = store_->Lookup(symbol);
}

void Builder::SetLink(const Object &name, Text symbol) {
  NamedSlot(name.handle())->value = store_->Lookup(symbol);
}

void Builder::SetLink(const Name &name, Text symbol) {
  NamedSlot(name.Lookup(store_))->value = store_->Lookup(symbol);
}

void Builder::SetLink(Text name, Text symbol) {
  NamedSlot(store_->Lookup(name))->value = store_->Lookup(symbol);
}

Frame Builder::Create() const {
  Handle h = store_->AllocateFrame(slots_.base(), slots_.end(), handle_);
  return Frame(store_, h);
}

void Builder::Update() const {
  store_->AllocateFrame(slots_.base(), slots_.end(), handle_);
}

void Builder::GetReferences(Range *range) {
  range->begin = reinterpret_cast<Handle *>(slots_.base());
  range->end = reinterpret_cast<Handle *>(slots_.end());
}

}  // namespace sling

