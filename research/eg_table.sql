-- 学生成绩信息表(sc)
CREATE TABLE sc (
    sno VARCHAR(10),    -- 学号
    cno VARCHAR(10),    -- 课程号
    score INT           -- 成绩
);

-- 课程信息表(course)
CREATE TABLE course (
    cno VARCHAR(10),        -- 课程号
    cname VARCHAR(10),      -- 课程名称
    credit INT,             -- 学分
    priorcourse VARCHAR(10) -- 前置课程
);

-- 班级信息表(class)
CREATE TABLE class (
    classno VARCHAR(10),        -- 班级编号
    classname VARCHAR(10),      -- 班级名称
    gno  VARCHAR(10)
);

-- 学生基表信息表(student)
CREATE TABLE student (
    sno VARCHAR(10),        -- 学号
    sname VARCHAR(10),      -- 学生姓名
    gender  VARCHAR(2),     -- 性别
    age INT,                -- 年龄
    nation VARCHAR(10),     -- 国籍
    classno VARCHAR(10)     -- 班级编号
);

-- 示例SQL
SELECT classno, classname, avg(score) as avg_score
FROM sc, (SELECT * FROM class WHERE class.gno='grade one') as sub
WHERE sc.sno IN (SELECT sno FROM student WHERE student.classno=sub.classno)
AND sc.cno IN (SELECT course.cno FROM course WHERE course.cname='computer')
GROUP BY classno, classname
HAVING avg(score) > 60
ORDER BY avg_score;


-- 非关联子查询
--- 想要知道哪个球员的身高最高，最高身高是多少
SELECT player_name, height
FROM player
WHERE height = (SELECT max(height) FROM player)

-- 关联子查询
--- 想要查找每个球队中大于平均身高的球员有哪些，并显示他们的球员姓名、身高以及所在球队ID
SELECT player_name, height, team_id
FROM player AS a
WHERE height > (SELECT avg(height) FROM player AS b WHERE a.team_id = b.team_id)

-- SELECT SQL书写顺序
SELECT ... FROM ... WHERE ... GROUP BY ... HAVING ... ORDER BY ...

-- SELECT SQL执行顺序
SELECT DISTINCT player_id, player_name, count(*) as num --顺序5
FROM player JOIN team ON player.team_id = team.team_id --顺序1
WHERE height > 1.80 --顺序2
GROUP BY player.team_id --顺序3
HAVING num > 2 --顺序4
ORDER BY num DESC --顺序6
LIMIT 2 --顺序7