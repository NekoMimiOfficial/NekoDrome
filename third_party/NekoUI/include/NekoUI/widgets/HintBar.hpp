#pragma once

#include "Flex.hpp"
#include "Label.hpp"

namespace neko {

class HintBar : public ContainerT<HintBar> {
 public:
  const char* typeName() const override { return "hintbar"; }

  static Ptr make() { return std::make_shared<HintBar>(); }

  bool interactive() const override { return false; }
  Widget* hitTest(Vec2) override { return nullptr; }

  void tick(Context& ctx, float) override {
    if (rev_ == ctx.hintRevision) return;
    rev_ = ctx.hintRevision;
    clearChildren();
    for (const Hint& h : ctx.hints) {
      GlyphSpec g = ctx.input->glyphFor(h.action);
      auto key = Label::make(g.text)->cls("hint-key")->fontFace(g.face);
      auto txt = Label::make(h.label)->cls("hint-label");
      auto item = Box::make(Direction::Row)->cls("hint");
      item->add(key)->add(txt);
      addChild(item);
    }
  }

 private:
  uint32_t rev_ = ~0u;
};

}
