from manim import *
import numpy as np


class PythonInteractiveLesson(Scene):
    def construct(self):

        # ==================================================
        # 1. SETTINGS & THEME COLORS
        # ==================================================

        self.camera.background_color = "#101722"

        WHITE = "#F4F7FB"
        BLUE = "#62B5FF"
        GREEN = "#48E0A4"
        YELLOW = "#FFD166"
        RED = "#FF6B6B"
        PURPLE = "#C792EA"
        MUTED = "#AAB6C6"
        GREY = "#536174"
        CARD_BG = "#162030"
        BORDER_COLOR = "#30445B"

        SINHALA_FONT = "Noto Sans Sinhala"
        ENG_FONT = "DejaVu Sans"
        MONO_FONT = "DejaVu Sans Mono"

        def si(text, size=22, color=WHITE):
            return Text(
                text,
                font=SINHALA_FONT,
                font_size=size,
                color=color,
            )

        def en(text, size=22, color=WHITE, weight=NORMAL):
            return Text(
                text,
                font=ENG_FONT,
                font_size=size,
                color=color,
                weight=weight,
            )

        def code_txt(text, size=21, color=WHITE):
            return Text(
                text,
                font=MONO_FONT,
                font_size=size,
                color=color,
            )

        def pause(seconds=1.0):
            self.wait(seconds)

        # ==================================================
        # 2. HEADER & CONVERSATIONAL TEACHER BANNER
        # ==================================================

        title = si(
            "පයිතන් කේතය පියවරෙන් පියවර තේරුම් ගනිමු",
            size=28,
            color=BLUE,
        )
        if title.width > 12.0:
            title.scale_to_fit_width(12.0)
        title.to_edge(UP, buff=0.22)

        underline = Line(
            LEFT * 6.2,
            RIGHT * 6.2,
            color=BORDER_COLOR,
            stroke_width=1.8,
        ).next_to(title, DOWN, buff=0.10)

        self.play(
            Write(title),
            Create(underline),
            run_time=1.4,
        )

        banner_box = RoundedRectangle(
            width=12.4,
            height=0.72,
            corner_radius=0.12,
            color=BORDER_COLOR,
            fill_color=CARD_BG,
            fill_opacity=0.95,
            stroke_width=1.6,
        ).move_to([0, 2.38, 0])

        self.play(FadeIn(banner_box), run_time=0.4)

        current_banner_text = VGroup()

        def say(text_str, color=YELLOW, rt=0.5):
            nonlocal current_banner_text
            new_txt = si(text_str, size=20, color=color)
            if new_txt.width > 11.8:
                new_txt.scale_to_fit_width(11.8)
            new_txt.move_to(banner_box.get_center())

            if len(current_banner_text) > 0:
                self.play(
                    FadeOut(current_banner_text, shift=UP * 0.08),
                    FadeIn(new_txt, shift=UP * 0.08),
                    run_time=rt,
                )
            else:
                self.play(
                    FadeIn(new_txt, shift=UP * 0.08),
                    run_time=rt,
                )
            current_banner_text = VGroup(new_txt)

        # ==================================================
        # 3. LEFT PANEL: PYTHON CODE BLOCK
        # ==================================================

        code_card = RoundedRectangle(
            width=4.9,
            height=5.15,
            corner_radius=0.14,
            color=BORDER_COLOR,
            fill_color=CARD_BG,
            fill_opacity=0.9,
            stroke_width=1.8,
        ).move_to([-3.95, -0.78, 0])

        raw_lines = [
            (0, 'x = "01101"'),
            (0, "t = 0"),
            (0, "p = 0"),
            (0, "for i in x:"),
            (1, "if (int(i)):"),
            (2, "t = t + 2 ** p"),
            (1, "p = p + 1"),
            (0, "print(t)"),
        ]

        LINE_YS = np.linspace(1.32, -2.85, len(raw_lines))
        BASE_CODE_X = -5.75
        INDENT_STEP = 0.40

        code_line_objs = VGroup()
        line_num_objs = VGroup()

        for idx, (indent, text_str) in enumerate(raw_lines):
            y = LINE_YS[idx]
            l_num = code_txt(f"{idx + 1}", size=15, color=GREY)
            l_num.move_to([-6.12, y, 0])
            line_num_objs.add(l_num)

            c_obj = code_txt(text_str, size=19, color=WHITE)
            c_obj.move_to(
                [BASE_CODE_X + indent * INDENT_STEP, y, 0],
                aligned_edge=LEFT,
            )
            code_line_objs.add(c_obj)

        self.play(
            FadeIn(code_card),
            FadeIn(line_num_objs),
            FadeIn(code_line_objs, shift=RIGHT * 0.1),
            run_time=0.8,
        )

        # ==================================================
        # PHASE 1: BREAKING DOWN THE CODE STRUCTURE
        # ==================================================

        say("මුලින්ම අපි මේ කේතයේ ප්‍රධාන කොටස් 3 මොනවාද කියලා හඳුනා ගනිමු.", YELLOW)
        pause(1.2)

        brace1 = BraceBetweenPoints(
            [-1.55, LINE_YS[0] + 0.15, 0],
            [-1.55, LINE_YS[2] - 0.15, 0],
            direction=RIGHT,
            color=BLUE,
        )
        desc1_card = RoundedRectangle(
            width=6.2,
            height=1.15,
            corner_radius=0.1,
            color=BLUE,
            fill_color=CARD_BG,
            fill_opacity=0.9,
            stroke_width=1.6,
        ).move_to([2.65, LINE_YS[1], 0])

        desc1_t1 = si("1. ආරම්භක විචල්‍යයන් (Variables)", size=19, color=BLUE)
        desc1_t2 = si('දත්ත ගබඩා කර තබා ගන්නා පෙට්ටි 3ක් (x, t සහ p)', size=16, color=WHITE)
        desc1_grp = VGroup(desc1_t1, desc1_t2).arrange(DOWN, buff=0.08).move_to(desc1_card)

        say("පළමු පේළි 3න් කරන්නේ x, t සහ p කියන විචල්‍යයන් (Variables) හඳුන්වා දීමයි.", BLUE)
        self.play(
            GrowFromCenter(brace1),
            FadeIn(desc1_card, shift=RIGHT * 0.15),
            FadeIn(desc1_grp, shift=RIGHT * 0.15),
            run_time=0.7,
        )
        pause(1.8)

        brace2 = BraceBetweenPoints(
            [-1.55, LINE_YS[3] + 0.15, 0],
            [-1.55, LINE_YS[6] - 0.15, 0],
            direction=RIGHT,
            color=GREEN,
        )
        desc2_card = RoundedRectangle(
            width=6.2,
            height=1.35,
            corner_radius=0.1,
            color=GREEN,
            fill_color=CARD_BG,
            fill_opacity=0.9,
            stroke_width=1.6,
        ).move_to([2.65, (LINE_YS[3] + LINE_YS[6]) / 2, 0])

        desc2_t1 = si("2. For Loop එක (නැවත නැවත ක්‍රියාත්මක වන කොටස)", size=18, color=GREEN)
        desc2_t2 = si('x හි ඇති අකුරු 5 සඳහාම පියවරෙන් පියවර ක්‍රියාත්මක වේ', size=16, color=WHITE)
        desc2_grp = VGroup(desc2_t1, desc2_t2).arrange(DOWN, buff=0.08).move_to(desc2_card)

        say("4 සිට 7 දක්වා පේළි වල තියෙන්නේ For Loop එකක්. මේක තමයි වැදගත්ම කොටස!", GREEN)
        self.play(
            GrowFromCenter(brace2),
            FadeIn(desc2_card, shift=RIGHT * 0.15),
            FadeIn(desc2_grp, shift=RIGHT * 0.15),
            run_time=0.7,
        )
        pause(2.0)

        brace3 = BraceBetweenPoints(
            [-1.55, LINE_YS[7] + 0.18, 0],
            [-1.55, LINE_YS[7] - 0.18, 0],
            direction=RIGHT,
            color=YELLOW,
        )
        desc3_card = RoundedRectangle(
            width=6.2,
            height=0.90,
            corner_radius=0.1,
            color=YELLOW,
            fill_color=CARD_BG,
            fill_opacity=0.9,
            stroke_width=1.6,
        ).move_to([2.65, LINE_YS[7], 0])

        desc3_t1 = si("3. අවසාන ප්‍රතිදානය (Output): t හි අගය පෙන්වයි", size=18, color=YELLOW)
        desc3_t1.move_to(desc3_card)

        say("අන්තිමට ලූපය ඉවර වුණාම, 8 වෙනි පේළියෙන් අවසාන පිළිතුර (t) පෙන්වනවා.", YELLOW)
        self.play(
            GrowFromCenter(brace3),
            FadeIn(desc3_card, shift=RIGHT * 0.15),
            FadeIn(desc3_t1, shift=RIGHT * 0.15),
            run_time=0.7,
        )
        pause(2.2)

        self.play(
            FadeOut(brace1),
            FadeOut(desc1_card),
            FadeOut(desc1_grp),
            FadeOut(brace2),
            FadeOut(desc2_card),
            FadeOut(desc2_grp),
            FadeOut(brace3),
            FadeOut(desc3_card),
            FadeOut(desc3_t1),
            run_time=0.7,
        )

        # ==================================================
        # PHASE 2: VISUALIZING HOW FOR LOOP & IF WORK
        # ==================================================

        say("දැන් බලමු 'for i in x:' කියන ලූපය ඇත්තටම වැඩ කරන්නේ කොහොමද කියලා.", BLUE)

        concept_card = RoundedRectangle(
            width=7.1,
            height=5.15,
            corner_radius=0.14,
            color=BORDER_COLOR,
            fill_color=CARD_BG,
            fill_opacity=0.9,
            stroke_width=1.8,
        ).move_to([2.45, -0.78, 0])

        self.play(FadeIn(concept_card), run_time=0.5)

        demo_x_lbl = si('x = "01101" තුළ අනුලකුණු 5ක් තියෙනවා:', size=18, color=MUTED)
        demo_x_lbl.move_to([2.45, 1.30, 0])

        demo_chars = ["0", "1", "1", "0", "1"]
        DEMO_XS = np.linspace(0.55, 4.35, 5)
        DEMO_Y = 0.55

        demo_tiles = VGroup()
        for idx, ch in enumerate(demo_chars):
            box = RoundedRectangle(
                width=0.72,
                height=0.72,
                corner_radius=0.1,
                color=BLUE,
                fill_color="#1D2C42",
                fill_opacity=1.0,
                stroke_width=2.0,
            ).move_to([DEMO_XS[idx], DEMO_Y, 0])
            txt = code_txt(f'"{ch}"', size=20, color=WHITE)
            txt.move_to(box)
            demo_tiles.add(VGroup(box, txt))

        i_tray_box = RoundedRectangle(
            width=1.7,
            height=1.05,
            corner_radius=0.12,
            color=YELLOW,
            fill_color=CARD_BG,
            fill_opacity=1.0,
            stroke_width=2.2,
        ).move_to([2.45, -1.10, 0])

        i_tray_lbl = si("i විචල්‍යය", size=17, color=YELLOW)
        i_tray_lbl.move_to([2.45, -0.75, 0])

        i_tray_val = code_txt("?", size=24, color=WHITE)
        i_tray_val.move_to([2.45, -1.25, 0])

        loop_explain_note = si(
            "ලූපය වට 5ක් කැරකෙමින් වමේ සිට දකුණට එකින් එක අකුර i තුළට ගනියි",
            size=16,
            color=GREEN,
        )
        if loop_explain_note.width > 6.6:
            loop_explain_note.scale_to_fit_width(6.6)
        loop_explain_note.move_to([2.45, -2.15, 0])

        self.play(
            FadeIn(demo_x_lbl),
            *[FadeIn(t, scale=0.85) for t in demo_tiles],
            FadeIn(i_tray_box),
            FadeIn(i_tray_lbl),
            FadeIn(i_tray_val),
            FadeIn(loop_explain_note),
            run_time=0.8,
        )

        say("ලූපයේ හැම වටයකදීම x ගේ ඊළඟ අකුර පිටපත් වෙලා 'i' කියන පෙට්ටියට වැටෙනවා!", GREEN)

        for demo_i in range(3):
            ch_val = demo_chars[demo_i]
            flying_copy = demo_tiles[demo_i][1].copy()
            target_txt = code_txt(f'"{ch_val}"', size=24, color=YELLOW)
            target_txt.move_to([2.45, -1.25, 0])

            self.play(
                Indicate(demo_tiles[demo_i][0], color=YELLOW, scale_factor=1.12),
                run_time=0.3,
            )
            self.play(
                Transform(flying_copy, target_txt, path_arc=-PI / 5),
                FadeOut(i_tray_val),
                run_time=0.55,
            )
            i_tray_val = flying_copy
            pause(0.35)

        pause(0.8)

        self.play(
            FadeOut(demo_x_lbl),
            FadeOut(demo_tiles),
            FadeOut(i_tray_box),
            FadeOut(i_tray_lbl),
            FadeOut(i_tray_val),
            FadeOut(loop_explain_note),
            run_time=0.5,
        )

        say("ඊළඟට 'if (int(i)):' කියන පේළියෙන් මොකද වෙන්නේ කියලා බලමු.", YELLOW)

        if_title = code_txt("if (int(i)):", size=24, color=YELLOW)
        if_title.move_to([2.45, 1.25, 0])

        if_sub = si(
            'int(i) මඟින් "0" හෝ "1" අකුරක් සංඛ්‍යාවක් (Integer) බවට පත් කරයි',
            size=17,
            color=MUTED,
        )
        if if_sub.width > 6.6:
            if_sub.scale_to_fit_width(6.6)
        if_sub.move_to([2.45, 0.65, 0])

        rule0_card = RoundedRectangle(
            width=6.3,
            height=1.15,
            corner_radius=0.1,
            color=RED,
            fill_color="#231822",
            fill_opacity=0.95,
            stroke_width=1.8,
        ).move_to([2.45, -0.35, 0])

        r0_t1 = si('i = "0" නම්  ->  int("0") = 0  ->  අසත්‍යයි (False)', size=18, color=RED)
        r0_t2 = si("එවිට t = t + 2 ** p පේළිය ක්‍රියාත්මක වන්නේ නැත (මඟ හරියි)", size=15, color=WHITE)
        r0_grp = VGroup(r0_t1, r0_t2).arrange(DOWN, buff=0.08).move_to(rule0_card)

        rule1_card = RoundedRectangle(
            width=6.3,
            height=1.15,
            corner_radius=0.1,
            color=GREEN,
            fill_color="#142928",
            fill_opacity=0.95,
            stroke_width=1.8,
        ).move_to([2.45, -1.80, 0])

        r1_t1 = si('i = "1" නම්  ->  int("1") = 1  ->  සත්‍යයි (True)', size=18, color=GREEN)
        r1_t2 = si("එවිට පමණක් t = t + 2 ** p පේළිය ක්‍රියාත්මක වී t ට අගය එකතු වේ!", size=15, color=WHITE)
        r1_grp = VGroup(r1_t1, r1_t2).arrange(DOWN, buff=0.08).move_to(rule1_card)

        self.play(
            FadeIn(if_title),
            FadeIn(if_sub),
            run_time=0.6,
        )
        pause(0.6)

        say("Python වලදී if(0) කියන්නේ False. ඒ නිසා i = 0 වුණොත් එකතු කිරීම මඟ හරිනවා.", RED)
        self.play(
            FadeIn(rule0_card, shift=UP * 0.1),
            FadeIn(r0_grp, shift=UP * 0.1),
            run_time=0.7,
        )
        pause(2.2)

        say("හැබැයි if(1) කියන්නේ True! ඒ නිසා i = 1 වුණොත් විතරක් t ට 2**p එකතු වෙනවා.", GREEN)
        self.play(
            FadeIn(rule1_card, shift=UP * 0.1),
            FadeIn(r1_grp, shift=UP * 0.1),
            run_time=0.7,
        )
        pause(2.4)

        self.play(
            FadeOut(concept_card),
            FadeOut(if_title),
            FadeOut(if_sub),
            FadeOut(rule0_card),
            FadeOut(r0_grp),
            FadeOut(rule1_card),
            FadeOut(r1_grp),
            run_time=0.7,
        )

        # ==================================================
        # PHASE 3: LIVE STEP-BY-STEP LOOP EXECUTION
        # ==================================================

        say("දැන් අපි මුල ඉඳන්ම කේතය පේළියෙන් පේළිය වැඩ කරන හැටි බලමු!", YELLOW)

        line_highlight = RoundedRectangle(
            width=4.65,
            height=0.40,
            corner_radius=0.06,
            color=YELLOW,
            fill_color=YELLOW,
            fill_opacity=0.15,
            stroke_width=1.6,
        )
        line_pointer = Triangle(
            color=YELLOW,
            fill_color=YELLOW,
            fill_opacity=1.0,
        ).scale(0.10).rotate(-PI / 2)

        pointer_shown = [False]

        def highlight_line(idx, col=YELLOW, rt=0.35):
            y = LINE_YS[idx]
            if not pointer_shown[0]:
                line_highlight.move_to([-3.92, y, 0]).set_color(col)
                line_pointer.move_to([-6.30, y, 0]).set_color(col)
                self.play(
                    FadeIn(line_highlight),
                    FadeIn(line_pointer),
                    run_time=rt,
                )
                pointer_shown[0] = True
            else:
                self.play(
                    line_highlight.animate.move_to([-3.92, y, 0]).set_color(col),
                    line_pointer.animate.move_to([-6.30, y, 0]).set_color(col),
                    run_time=rt,
                )

        CHAR_XS = np.linspace(0.45, 4.45, 5)
        CHAR_Y = 1.25

        x_title_lbl = si('x = "01101"', size=17, color=MUTED)
        x_title_lbl.move_to([-0.55, CHAR_Y, 0])

        char_tiles = VGroup()
        for idx, ch in enumerate(demo_chars):
            box = RoundedRectangle(
                width=0.70,
                height=0.70,
                corner_radius=0.1,
                color=BORDER_COLOR,
                fill_color=CARD_BG,
                fill_opacity=0.95,
                stroke_width=2.0,
            ).move_to([CHAR_XS[idx], CHAR_Y, 0])
            txt = code_txt(f'"{ch}"', size=20, color=WHITE)
            txt.move_to(box)
            char_tiles.add(VGroup(box, txt))

        VAR_Y = -0.15
        VAR_XS = [0.25, 2.45, 4.65]
        var_specs = [
            ("i (අකුර)", "-", BLUE),
            ("t (එකතුව)", "0", GREEN),
            ("p (බලය)", "0", YELLOW),
        ]

        var_cards = VGroup()
        var_val_objs = []

        for idx, (v_label, init_str, v_col) in enumerate(var_specs):
            vx = VAR_XS[idx]
            card = RoundedRectangle(
                width=1.95,
                height=1.08,
                corner_radius=0.12,
                color=v_col,
                fill_color=CARD_BG,
                fill_opacity=0.9,
                stroke_width=2.0,
            ).move_to([vx, VAR_Y, 0])

            lbl = si(v_label, size=15, color=v_col)
            lbl.move_to([vx, VAR_Y + 0.30, 0])

            val_txt = en(init_str, size=25, color=WHITE, weight=BOLD)
            val_txt.move_to([vx, VAR_Y - 0.16, 0])

            var_cards.add(VGroup(card, lbl))
            var_val_objs.append(val_txt)

        board_card = RoundedRectangle(
            width=6.8,
            height=2.05,
            corner_radius=0.14,
            color=BORDER_COLOR,
            fill_color=CARD_BG,
            fill_opacity=0.92,
            stroke_width=1.8,
        ).move_to([2.45, -2.18, 0])

        board_content = VGroup()

        def update_board(line1, line2=None, rt=0.4):
            nonlocal board_content
            new_grp = VGroup()
            if line1.width > 6.3:
                line1.scale_to_fit_width(6.3)

            if line2 is None:
                line1.move_to([2.45, -2.18, 0])
                new_grp.add(line1)
            else:
                if line2.width > 6.3:
                    line2.scale_to_fit_width(6.3)
                line1.move_to([2.45, -1.85, 0])
                line2.move_to([2.45, -2.48, 0])
                new_grp.add(line1, line2)

            if len(board_content) > 0:
                self.play(
                    FadeOut(board_content, shift=UP * 0.06),
                    FadeIn(new_grp, shift=UP * 0.06),
                    run_time=rt,
                )
            else:
                self.play(FadeIn(new_grp, shift=UP * 0.06), run_time=rt)
            board_content = new_grp

        def set_var(v_idx, new_str, col, rt=0.45):
            vx = VAR_XS[v_idx]
            new_v = en(new_str, size=25, color=WHITE, weight=BOLD)
            new_v.move_to([vx, VAR_Y - 0.16, 0])
            self.play(
                Indicate(var_cards[v_idx][0], color=col, scale_factor=1.06),
                Transform(var_val_objs[v_idx], new_v),
                run_time=rt,
            )

        self.play(FadeIn(board_card), run_time=0.4)

        highlight_line(0, BLUE)
        say('1 වන පේළිය: x විචල්‍යයට "01101" ආදේශ වෙනවා.', BLUE)
        update_board(si('x = "01101" ලෙස තැන්පත් විය', 19, BLUE))
        self.play(
            FadeIn(x_title_lbl),
            *[FadeIn(t, scale=0.85) for t in char_tiles],
            FadeIn(var_cards[0]),
            FadeIn(var_val_objs[0]),
            run_time=0.7,
        )
        pause(0.8)

        highlight_line(1, GREEN)
        say("2 වන පේළිය: එකතුව හදන්න ගන්න t විචල්‍යය 0 ලෙස ආරම්භ වෙනවා.", GREEN)
        update_board(si("t = 0 (ආරම්භක අගය)", 19, GREEN))
        self.play(
            FadeIn(var_cards[1], scale=0.9),
            FadeIn(var_val_objs[1], scale=0.9),
            run_time=0.6,
        )
        pause(0.8)

        highlight_line(2, YELLOW)
        say("3 වන පේළිය: 2 හි බලයන් ගණනය කරන p විචල්‍යයත් 0 ලෙස ආරම්භ වෙනවා.", YELLOW)
        update_board(si("p = 0 (ආරම්භක බලය)", 19, YELLOW))
        self.play(
            FadeIn(var_cards[2], scale=0.9),
            FadeIn(var_val_objs[2], scale=0.9),
            run_time=0.6,
        )
        pause(0.9)

        loop_back_arrow = CurvedArrow(
            start_point=[-1.75, LINE_YS[6], 0],
            end_point=[-1.75, LINE_YS[3], 0],
            angle=PI / 2.2,
            color=PURPLE,
            stroke_width=2.8,
            tip_length=0.18,
        )

        char_pointer = Arrow(
            start=[CHAR_XS[0], CHAR_Y + 0.85, 0],
            end=[CHAR_XS[0], CHAR_Y + 0.38, 0],
            color=BLUE,
            stroke_width=3.0,
            buff=0.0,
            max_tip_length_to_length_ratio=0.32,
        )
        ptr_visible = False

        t_val = 0
        p_val = 0

        for step_idx, ch in enumerate(demo_chars):
            round_num = step_idx + 1

            highlight_line(3, BLUE)
            say(
                f'වටය {round_num}: ලූපය x ගේ {round_num}-වන අකුර ("{ch}") අරගෙන i පෙට්ටියට දානවා.',
                BLUE,
            )

            if not ptr_visible:
                self.play(
                    GrowArrow(char_pointer),
                    char_tiles[step_idx][0].animate.set_color(BLUE),
                    run_time=0.4,
                )
                ptr_visible = True
            else:
                self.play(
                    char_pointer.animate.move_to([CHAR_XS[step_idx], CHAR_Y + 0.61, 0]),
                    char_tiles[step_idx - 1][0].animate.set_color(GREY),
                    char_tiles[step_idx][0].animate.set_color(BLUE),
                    run_time=0.4,
                )

            flying_ch = char_tiles[step_idx][1].copy()
            new_i_target = en(f'"{ch}"', size=25, color=WHITE, weight=BOLD)
            new_i_target.move_to([VAR_XS[0], VAR_Y - 0.16, 0])

            update_board(
                si(f"වටය {round_num} (Iteration {round_num}):", 18, BLUE),
                code_txt(f'i = "{ch}"', 22, WHITE),
            )

            self.play(
                ReplacementTransform(flying_ch, new_i_target, path_arc=-PI / 5),
                FadeOut(var_val_objs[0]),
                Indicate(var_cards[0][0], color=BLUE, scale_factor=1.06),
                run_time=0.65,
            )
            var_val_objs[0] = new_i_target
            pause(0.7)

            highlight_line(4, YELLOW)
            is_one = int(ch) == 1

            if is_one:
                say(
                    f'දැන් i = "{ch}". int("1") කියන්නේ 1 (True) නිසා if කොටස ඇතුළට යනවා!',
                    GREEN,
                )
                update_board(
                    si(f'int("{ch}") = 1 -> කොන්දේසිය සත්‍යයි (True ✓)', 19, GREEN),
                    si("එනිසා 6 වන පේළිය ක්‍රියාත්මක වේ!", 17, WHITE),
                )
                pause(1.0)

                highlight_line(5, GREEN)
                add_val = 2 ** p_val
                new_t = t_val + add_val

                say(
                    f"දැන් t ට 2^{p_val} (එනම් {add_val}) එකතු වෙලා t = {new_t} බවට පත් වෙනවා.",
                    GREEN,
                )
                math_line = MarkupText(
                    rf"t = {t_val} + 2<sup>{p_val}</sup> = {t_val} + {add_val} = {new_t}",
                    font="DejaVu Sans",
                    font_size=30,
                    color=GREEN,
                )
                update_board(
                    si("t හි අගය යාවත්කාලීන වේ:", 18, GREEN),
                    math_line,
                )
                t_val = new_t
                set_var(1, str(t_val), GREEN, rt=0.55)
                pause(1.1)

            else:
                say(
                    f'දැන් i = "{ch}". int("0") කියන්නේ 0 (False) නිසා එකතු කිරීමේ පේළිය මඟ හරිනවා!',
                    RED,
                )
                update_board(
                    si(f'int("{ch}") = 0 -> කොන්දේසිය අසත්‍යයි (False ✗)', 19, RED),
                    si("t වෙනස් නොවේ (6 වන පේළිය මඟ හරියි)", 17, MUTED),
                )
                pause(1.2)

            highlight_line(6, YELLOW)
            new_p = p_val + 1
            say(
                f"ඊළඟ වටයට යන්න කලින් 7 වන පේළියෙන් p හි අගය 1කින් වැඩි වෙනවා (p = {new_p}).",
                YELLOW,
            )
            p_math = MarkupText(
                rf"p = {p_val} + 1 = {new_p}",
                font="DejaVu Sans",
                font_size=30,
                color=YELLOW,
            )
            update_board(
                si("හැම වටයකම අවසානයේ p එකකින් වැඩි වේ:", 18, YELLOW),
                p_math,
            )
            p_val = new_p
            set_var(2, str(p_val), YELLOW, rt=0.5)
            pause(0.8)

            if step_idx < len(demo_chars) - 1:
                say("ලූපයේ මේ වටය ඉවරයි! දැන් ආයෙත් උඩට ගිහින් ඊළඟ අකුර ගන්නවා.", PURPLE)
                self.play(Create(loop_back_arrow), run_time=0.45)
                pause(0.4)
                self.play(FadeOut(loop_back_arrow), run_time=0.3)

        self.play(
            char_tiles[-1][0].animate.set_color(GREY),
            FadeOut(char_pointer),
            run_time=0.4,
        )

        highlight_line(7, GREEN)
        say(
            f"අකුරු 5ම කියවලා ඉවරයි! දැන් ලූපයෙන් එළියට ඇවිත් print(t) මඟින් {t_val} පෙන්වනවා.",
            GREEN,
        )
        update_board(
            si("ලූපය අවසන්! print(t) ක්‍රියාත්මක වේ:", 18, GREEN),
            si(f"අවසාන ප්‍රතිදානය (Output) = {t_val}", 24, YELLOW),
        )

        t_glow = SurroundingRectangle(
            var_cards[1][0],
            color=GREEN,
            buff=0.08,
            corner_radius=0.14,
            stroke_width=3.2,
        )
        self.play(
            Create(t_glow),
            var_val_objs[1].animate.scale(1.22).set_color(GREEN),
            run_time=0.8,
        )
        pause(2.5)

        # ==================================================
        # PHASE 4: FINAL TAKEAWAY SUMMARY
        # ==================================================

        self.play(
            FadeOut(code_card),
            FadeOut(line_num_objs),
            FadeOut(code_line_objs),
            FadeOut(line_highlight),
            FadeOut(line_pointer),
            FadeOut(x_title_lbl),
            FadeOut(char_tiles),
            FadeOut(var_cards),
            *[FadeOut(v) for v in var_val_objs],
            FadeOut(t_glow),
            FadeOut(board_card),
            FadeOut(board_content),
            run_time=0.9,
        )

        say("සරලවම කිව්වොත් මේ කේතයෙන් වුණේ මේ ගණනය කිරීමයි:", BLUE)

        final_card = RoundedRectangle(
            width=11.0,
            height=4.0,
            corner_radius=0.16,
            color=BLUE,
            fill_color=CARD_BG,
            fill_opacity=0.92,
            stroke_width=2.2,
        ).move_to([0, -0.55, 0])

        f_line1 = si(
            'x = "01101" හි "1" ඇති ස්ථානවලට අදාළ 2 හි බලයන් පමණක් එකතු විය:',
            size=20,
            color=WHITE,
        )
        f_line2 = MarkupText(
            "Index (p):&#160;&#160;p=0 &#8594; 0,&#160;&#160;p=1 &#8594; 2<sup>1</sup>,&#160;&#160;p=2 &#8594; 2<sup>2</sup>,&#160;&#160;p=3 &#8594; 0,&#160;&#160;p=4 &#8594; 2<sup>4</sup>",
            font="DejaVu Sans",
            font_size=27,
            color=MUTED,
        )
        f_line3 = MarkupText(
            "t = 2<sup>1</sup> + 2<sup>2</sup> + 2<sup>4</sup> = 2 + 4 + 16 = 22",
            font="DejaVu Sans",
            font_size=36,
            color=YELLOW,
        )
        f_line4 = si(
            "ප්‍රතිදානය (Final Output) = 22",
            size=26,
            color=GREEN,
        )

        final_grp = VGroup(
            f_line1,
            f_line2,
            f_line3,
            f_line4,
        ).arrange(DOWN, buff=0.34).move_to(final_card.get_center())

        self.play(FadeIn(final_card, scale=0.96), run_time=0.5)
        for item in final_grp:
            self.play(FadeIn(item, shift=UP * 0.12), run_time=0.6)
            pause(0.35)

        pause(3.5)

        self.play(
            FadeOut(final_card),
            FadeOut(final_grp),
            FadeOut(banner_box),
            FadeOut(current_banner_text),
            FadeOut(title),
            FadeOut(underline),
            run_time=1.1,
        )
        pause(0.5)
