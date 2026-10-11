# 单人 Yahtzee

项目启动场景是 `meow/game/yatzy.tscn`。构建后在 Godot 按 F6（打开该场景）或 F5（运行项目）即可游玩。界面代码是 `src/game/yatzy_view.h/.cpp`，使用现有 `YatzyGame`，不复制计分逻辑。

点击 ROLL 掷骰，点击骰子保留/释放，点击计分表的空白项查看选择，再点击 SCORE 提交。设置可查看规则和重新开始，13 回合结束后显示总分并可再玩一局。返回按钮确认后退出；道具数量为 0，当前没有道具功能。

使用 Yahtzee 的 13 项规则，5 颗六面骰，每回合最多掷 3 次，可随时保留/释放骰子，第一次掷骰后即可计分。每项只能填写一次，允许记零分；填满 13 项自动结束。上区至少 63 分奖励 35 分。

三条、四条按全部骰子之和计分；葫芦为三颗同点加两颗同点，25 分；小顺子为任意连续 4 点，30 分；大顺子为连续 5 点，40 分；五同为 50 分；机会为全部点数之和。

五同格已填后再次掷出五同，必须先填写对应上区；该格已填则填写空的下区，葫芦/顺子采用固定 Joker 分；下区填满后在其他上区填零。原五同格为 50 分时每次额外奖励 100 分，为零分时不奖励。预览不发放奖励，提交计分才发放。

规则来源：[Hasbro Yahtzee](https://www.hasbro.com/common/instruct/yahtzee.pdf)。

## Godot 调用

构建扩展后可直接实例化 `YatzyGame`（RefCounted，无需场景节点）：

```gdscript
var game = YatzyGame.new()
game.state_changed.connect(func(): print(game.get_state()))
game.roll()
game.set_held(0, true) # 骰子索引 0–4
game.roll()
print(game.preview_score(12)) # 机会项
game.score(12) # 提交后清空骰子和保留状态，进入下一回合
```

`get_category_names()` 按索引 0–12 返回：ones、twos、threes、fours、fives、sixes、three_of_a_kind、four_of_a_kind、full_house、small_straight、large_straight、yahtzee、chance。

`get_state()` 返回独立快照：dice、held（0/1）、scores、previews、rolls_left、rounds_completed、finished、upper_total、upper_bonus、yahtzee_bonus、total。未掷骰时 dice 为 0；scores 的 -1 表示未填，0 表示已填零分；previews 的 -1 表示当前不可提交。非法操作返回 false，不修改游戏状态；reset() 开始新局。

## 验证

```text
python tests/run_yatzy_test.py
scons
python tests/run_yatzy_test.py --godot "Godot 控制台可执行文件路径"
python tests/run_yatzy_test.py --godot "Godot 控制台可执行文件路径" --visual
```

确定性核心测试覆盖计分、保留、三次上限、奖励、Joker、13 回合结束及重置。测试 cpp 只在 YATZY_TEST 定义时生成入口，不影响扩展构建。

可选的 Godot 测试使用 `tests/workspace/meow` 中的统一隔离工程验证反射注册、方法、状态快照、信号和界面按钮完成完整一局，不修改现有场景。`--visual` 额外渲染界面并保存截图到 `tests/workspace/meow/ui.png`。
