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

#include "nlp/parser/parser.h"

#include "frame/serialization.h"
#include "myelin/kernel/dragnn.h"
#include "myelin/kernel/tensorflow.h"

namespace sling {
namespace nlp {

void Parser::Load(Store *store, const string &model) {
  // Register kernels for implementing parser ops.
  RegisterTensorflowLibrary(&library_);
  RegisterDragnnLibrary(&library_);

  // Load and analyze parser flow file.
  myelin::Flow flow;
  CHECK(flow.Load(model));
  flow.Analyze(library_);

  // Compile parser flow.
  CHECK(network_.Compile(flow, library_));

  // Get computation for each function.
  lr_ = GetCell("lr_lstm");
  rl_ = GetCell("rl_lstm");
  ff_ = GetCell("ff");

  // Get connectors.
  lr_control_ = GetConnector("lr_lstm/control");
  lr_hidden_ = GetConnector("lr_lstm/hidden");
  rl_control_ = GetConnector("rl_lstm/control");
  rl_hidden_ = GetConnector("rl_lstm/hidden");
  ff_step_ = GetConnector("ff/step");

  // Get LR LSTM parameters.
  lr_feature_words_ = GetParam("lr_lstm/words");
  lr_c_in_ = GetParam("lr_lstm/c_in");
  lr_c_out_ = GetParam("lr_lstm/c_out");
  lr_h_in_ = GetParam("lr_lstm/h_in");
  lr_h_out_ = GetParam("lr_lstm/h_out");

  // Get RL LSTM parameters.
  rl_feature_words_ = GetParam("rl_lstm/words");
  rl_c_in_ = GetParam("rl_lstm/c_in");
  rl_c_out_ = GetParam("rl_lstm/c_out");
  rl_h_in_ = GetParam("rl_lstm/h_in");
  rl_h_out_ = GetParam("rl_lstm/h_out");

  // Get FF parameters.
  ff_feature_lr_focus_ = GetParam("ff/lr");
  ff_feature_rl_focus_ = GetParam("ff/rl");
  ff_feature_lr_attention_ = GetParam("ff/frame-end-lr");
  ff_feature_rl_attention_ = GetParam("ff/frame-end-rl");
  ff_feature_frame_create_ = GetParam("ff/frame-creation-steps");
  ff_feature_frame_focus_ = GetParam("ff/frame-focus-steps");
  ff_feature_history_ = GetParam("ff/history");
  ff_feature_roles_ = GetParam("ff/roles");
  ff_lr_lstm_ = GetParam("ff/link/lr_lstm");
  ff_rl_lstm_ = GetParam("ff/link/rl_lstm");
  ff_steps_ = GetParam("ff/steps");
  ff_hidden_ = GetParam("ff/hidden");
  ff_output_ = GetParam("ff/output");

  // Get attention depth.
  attention_depth_ = ff_feature_lr_attention_->elements();
  CHECK_EQ(attention_depth_, ff_feature_rl_attention_->elements());
  CHECK_EQ(attention_depth_, ff_feature_frame_create_->elements());
  CHECK_EQ(attention_depth_, ff_feature_frame_focus_->elements());

  // Get history size.
  history_size_ = ff_feature_history_->elements();

  // Get maximum number of role features.
  max_roles_ = ff_feature_roles_->elements();

  // Load lexicon.
  myelin::Flow::Blob *vocabulary = flow.DataBlock("lexicon");
  CHECK(vocabulary != nullptr);
  lexicon_.Init(vocabulary);
  normalize_digits_ = vocabulary->attrs.Get("normalize_digits", false);
  oov_ = vocabulary->attrs.Get("oov", -1);

  // Load commons and action stores.
  myelin::Flow::Blob *commons = flow.DataBlock("commons");
  if (commons != nullptr) {
    StringDecoder decoder(store, commons->data, commons->size);
    decoder.DecodeAll();
  }
  myelin::Flow::Blob *actions = flow.DataBlock("actions");
  if (actions != nullptr) {
    StringDecoder decoder(store, actions->data, actions->size);
    decoder.DecodeAll();
  }

  // Initialize action table.
  store_ = store;
  actions_.Init(store);
  num_actions_ = actions_.NumActions();
  CHECK_GT(num_actions_, 0);

  // Get the set of roles that connect two frames.
  for (int i = 0; i < num_actions_; ++i) {
    const auto &action = actions_.Action(i);
    if (action.type == ParserAction::CONNECT ||
        action.type == ParserAction::EMBED ||
        action.type == ParserAction::ELABORATE) {
      if (roles_.find(action.role) == roles_.end()) {
        int index = roles_.size();
        roles_[action.role] = index;
      }
    }
  }

  // Compute the offsets for the four types of role features. These are laid
  // out in this order: all (i, r) features, all (r, j) features, all (i, j)
  // features, all (i, r, j) features.
  int combinations = frame_limit_ * roles_.size();
  outlink_offset_ = 0;
  inlink_offset_ = outlink_offset_ + combinations;
  unlabeled_link_offset_ = inlink_offset_ + combinations;
  labeled_link_offset_ = unlabeled_link_offset_ + frame_limit_ * frame_limit_;
}

int Parser::LookupWord(const string &word) const {
  // Lookup word in vocabulary.
  int id = lexicon_.Lookup(word);

  if (id == oov_ && normalize_digits_) {
    // Check if word has digits.
    bool has_digits = false;
    for (char c : word) if (c >= '0' && c <= '9') has_digits = true;

    if (has_digits) {
      // Normalize digits and lookup the normalized word.
      string normalized = word;
      for (char &c : normalized) if (c >= '0' && c <= '9') c = '9';
      id = lexicon_.Lookup(normalized);
    }
  }

  return id;
}

void Parser::Parse(Document *document) const {
  // Parse each sentence of the document.
  for (SentenceIterator s(document); s.more(); s.next()) {
    // Initialize parser model instance data.
    ParserInstance data(this, document, s.begin(), s.end());
    ParserState &state = data.state;

    // Look up words in vocabulary.
    for (int i = s.begin(); i < s.end(); ++i) {
      int word = LookupWord(document->token(i).text());
      data.words[i - s.begin()] = word;
    }

    // Compute left-to-right LSTM.
    for (int i = 0; i < s.length(); ++i) {
      // Attach hidden and control layers.
      data.lr.Clear();
      int in = i > 0 ? i - 1 : s.length();
      int out = i;
      data.AttachLR(in, out);

      // Extract features.
      data.ExtractFeaturesLR(out);

      // Compute LSTM cell.
      data.lr.Compute();
    }

    // Compute right-to-left LSTM.
    for (int i = 0; i < s.length(); ++i) {
      // Attach hidden and control layers.
      data.rl.Clear();
      int in = s.length() - i;
      int out = in - 1;
      data.AttachRL(in, out);

      // Extract features.
      data.ExtractFeaturesRL(out);

      // Compute LSTM cell.
      data.rl.Compute();
    }

    // Run FF to predict transitions.
    bool done = false;
    int steps_since_shift = 0;
    int step = 0;
    while (!done) {
      // Allocate space for next step.
      data.ff_step.push();

      // Attach instance to recurrent layers.
      data.ff.Clear();
      data.AttachFF(step);

      // Extract features.
      data.ExtractFeaturesFF(step);

      // Predict next action.
      data.ff.Compute();
      float *output = data.ff.Get<float>(ff_output_);
      int prediction = 0;
      float max_score = -INFINITY;
      for (int a = 0; a < num_actions_; ++a) {
        if (output[a] > max_score) {
          const ParserAction &action = actions_.Action(a);
          if (state.CanApply(action)) {
            prediction = a;
            max_score = output[a];
          }
        }
      }

      // Apply action to parser state.
      const ParserAction &action = actions_.Action(prediction);
      state.Apply(action);

      // Update state.
      switch (action.type) {
        case ParserAction::SHIFT:
          steps_since_shift = 0;
          break;

        case ParserAction::STOP:
          done = true;
          break;

        case ParserAction::EVOKE:
        case ParserAction::REFER:
        case ParserAction::CONNECT:
        case ParserAction::ASSIGN:
        case ParserAction::EMBED:
        case ParserAction::ELABORATE:
          steps_since_shift++;
          if (state.AttentionSize() > 0) {
            int focus = state.Attention(0);
            if (data.create_step.size() < focus + 1) {
              data.create_step.resize(focus + 1);
              data.create_step[focus] = step;
            }
            if (data.focus_step.size() < focus + 1) {
              data.focus_step.resize(focus + 1);
            }
            data.focus_step[focus] = step;
          }
      }

      // Next step.
      step += 1;
    }

    // Add frames for sentence to the document.
    state.AddParseToDocument(document);
  }
}

myelin::Cell *Parser::GetCell(const string &name) {
  myelin::Cell *cell = network_.GetCell(name);
  if (cell == nullptr) {
    LOG(FATAL) << "Unknown parser cell: " << name;
  }
  return cell;
}

myelin::Connector *Parser::GetConnector(const string &name) {
  myelin::Connector *cnx = network_.GetConnector(name);
  if (cnx == nullptr) {
    LOG(FATAL) << "Unknown parser connector: " << name;
  }
  return cnx;
}

myelin::Tensor *Parser::GetParam(const string &name) {
  myelin::Tensor *param = network_.GetParameter(name);
  if (param == nullptr) {
    LOG(FATAL) << "Unknown parser parameter: " << name;
  }
  return param;
}

ParserInstance::ParserInstance(const Parser *parser, Document *document,
                               int begin, int end)
    : parser(parser),
      state(document->store(), begin, end),
      lr(parser->lr_),
      rl(parser->rl_),
      ff(parser->ff_),
      lr_c(parser->lr_control_),
      lr_h(parser->lr_hidden_),
      rl_c(parser->rl_control_),
      rl_h(parser->rl_hidden_),
      ff_step(parser->ff_step_) {
  // Allocate space for word ids.
  int length = end - begin;
  words.resize(length);

  // Add one extra element to LSTM activations for boundary element.
  lr_c.resize(length + 1);
  lr_h.resize(length + 1);
  rl_c.resize(length + 1);
  rl_h.resize(length + 1);

  // Reserve two transitions per token.
  ff_step.reserve(length * 2);
}

void ParserInstance::AttachLR(int input, int output) {
  lr.Set(parser->lr_c_in_, &lr_c, input);
  lr.Set(parser->lr_c_out_, &lr_c, output);
  lr.Set(parser->lr_h_in_, &lr_h, input);
  lr.Set(parser->lr_h_out_, &lr_h, output);
}

void ParserInstance::AttachRL(int input, int output) {
  rl.Set(parser->rl_c_in_, &rl_c, input);
  rl.Set(parser->rl_c_out_, &rl_c, output);
  rl.Set(parser->rl_h_in_, &rl_h, input);
  rl.Set(parser->rl_h_out_, &rl_h, output);
}

void ParserInstance::AttachFF(int output) {
  ff.Set(parser->ff_lr_lstm_, &lr_h);
  ff.Set(parser->ff_rl_lstm_, &rl_h);
  ff.Set(parser->ff_steps_, &ff_step);
  ff.Set(parser->ff_hidden_, &ff_step, output);
}

void ParserInstance::ExtractFeaturesLR(int current) {
  int word = words[current];
  *lr.Get<int>(parser->lr_feature_words_) = word;
}

void ParserInstance::ExtractFeaturesRL(int current) {
  int word = words[current];
  *rl.Get<int>(parser->rl_feature_words_) = word;
}

void ParserInstance::ExtractFeaturesFF(int step) {
  // Compute LSTM focus features.
  int current = state.current() - state.begin();
  if (current == state.end()) current = -1;
  *ff.Get<int>(parser->ff_feature_lr_focus_) = current;
  *ff.Get<int>(parser->ff_feature_rl_focus_) = current;

  // Compute frame attention, create, and focus features.
  int *lr = ff.Get<int>(parser->ff_feature_lr_attention_);
  int *rl = ff.Get<int>(parser->ff_feature_rl_attention_);
  int *create = ff.Get<int>(parser->ff_feature_frame_create_);
  int *focus = ff.Get<int>(parser->ff_feature_frame_focus_);
  for (int d = 0; d < parser->attention_depth_; ++d) {
    int att = -2;
    int created = -2;
    int focused = -2;
    if (d < state.AttentionSize()) {
      // Get frame from attention buffer.
      int frame = state.Attention(d);

      // Get end token for phrase that evoked frame.
      att = state.FrameEvokeEnd(frame);
      if (att != -1) att -= state.begin() + 1;

      // Get the step numbers that created and focused the frame.
      if (frame < create_step.size()) {
        created = create_step[frame];
      }
      if (frame < focus_step.size()) {
        focused = focus_step[frame];
      }
    }
    lr[d] = att;
    rl[d] = att;
    create[d] = created;
    focus[d] = focused;
  }

  // Compute history feature.
  int *history = ff.Get<int>(parser->ff_feature_history_);
  int h = 0;
  int s = step - 1;
  while (h < parser->history_size_ && s >= 0) history[h++] = s--;
  while (h < parser->history_size_) history[h++] = -2;

  // Construct a mapping from absolute frame index -> attention index.
  std::unordered_map<int, int> frame_to_attention;
  for (int i = 0; i < parser->frame_limit_; ++i) {
    if (i < state.AttentionSize()) {
      frame_to_attention[state.Attention(i)] = i;
    } else {
      break;
    }
  }

  // Compute role features.
  int *roles = ff.Get<int>(parser->ff_feature_roles_);
  int r = 0;
  for (const auto &kv : frame_to_attention) {
    // Attention index of the source frame.
    int source = kv.second;
    int outlink_base = parser->outlink_offset_ + source * parser->roles_.size();

    // Go over each slot of the source frame.
    Handle handle = state.frame(kv.first);
    const FrameDatum *frame = state.store()->GetFrame(handle);
    for (const Slot *slot = frame->begin(); slot < frame->end(); ++slot) {
      const auto &it = parser->roles_.find(slot->name);
      if (it == parser->roles_.end()) continue;

      int role = it->second;
      if (r < parser->max_roles_) {
        // (source, role).
        roles[r++] = outlink_base + role;
      }
      if (slot->value.IsIndex()) {
        const auto &it2 = frame_to_attention.find(slot->value.AsIndex());
        if (it2 != frame_to_attention.end()) {
          // Attention index of the target frame.
          int target = it2->second;
          if (r < parser->max_roles_) {
            // (role, target)
            roles[r++] = parser->inlink_offset_ +
                         target * parser->roles_.size() +
                         role;
          }
          if (r < parser->max_roles_) {
            // (source, target)
            roles[r++] = parser->unlabeled_link_offset_ +
                         source * parser->frame_limit_ +
                         target;
          }
          if (r < parser->max_roles_) {
            // (source, role, target)
            roles[r++] = parser->labeled_link_offset_ +
                         source * parser->frame_limit_ * parser->roles_.size() +
                         target * parser->roles_.size() +
                         role;
          }
        }
      }
    }
  }
  while (r < parser->max_roles_) roles[r++] = -2;
}

}  // namespace nlp
}  // namespace sling

