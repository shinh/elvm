// The idea here is to do the wmach stuff but make it online instead of all at
// once

use std::fmt;
use std::fs::File;
use std::io::Read;
use std::path::Path;
use std::str::FromStr;

use anyhow::Result;
use thiserror::Error;

use nom::{
    branch::alt, bytes::complete::is_not, bytes::complete::tag, bytes::complete::take_while1,
    character::complete::multispace0, character::complete::multispace1,
    character::complete::space0, combinator::opt, multi::fold_many1, multi::many0, sequence::pair,
    sequence::separated_pair, sequence::tuple,
};

#[derive(Debug, Error)]
pub enum WmachErr {
    #[error("{message}")]
    GeneralError { message: String },

    /*
    #[error("Duplicate label: {label}")]
    DuplicateLabel { label: String },

    // this realy should be a LabelId but I don't know how to pull it out of the Target
    #[error("At instruction {offset} unknown target ``{target}'' referenced")]
    UnknownTarget { offset: InsnOffset, target: Target },
    */
    #[error("IO error: {err}")]
    IoError { err: std::io::Error },
}

impl From<std::io::Error> for WmachErr {
    fn from(error: std::io::Error) -> WmachErr {
        WmachErr::IoError { err: error }
    }
}

// This is what we get from a file
#[derive(Debug, Clone)]
pub enum Stmt {
    Write(WriteOp),
    Seek(SeekOp),
    Io(IoOp),
    Label(LabelId),
    Jmp(Target, Target),
    Debug,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum WriteOp {
    Set,
    Unset,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum SeekOp {
    Left(usize),
    Right(usize),
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum IoOp {
    In,
    Out,
}

pub type LabelId = String;
/*
pub type InsnOffset = usize;
pub type LabelMap = HashMap<LabelId, InsnOffset>;
*/
#[derive(Debug, Clone)]
pub enum Target {
    NextAddress,
    Name(LabelId),
}

impl fmt::Display for Target {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Target::NextAddress => write!(f, "<NextAddress>"),
            Target::Name(label) => write!(f, "{}", label),
        }
    }
}

pub type Code = Vec<Stmt>;

#[derive(Debug)]
pub struct Program {
    pub instructions: Code,
}

impl FromStr for Program {
    type Err = WmachErr;

    fn from_str(unparsed: &str) -> Result<Program, WmachErr> {
        let statements = Program::parse_statements(unparsed)?;

        Ok(Program {
            instructions: statements,
        })
    }
}

fn label(input: &str) -> nom::IResult<&str, &str> {
    take_while1(|input| {
        // misc = { "'" | '_' }
        // label_id = (alpha | digit | misc)+
        match input {
            'a'..='z' => true,
            'A'..='Z' => true,
            '0'..='9' => true,
            '\'' => true,
            '_' => true,
            _ => false,
        }
    })(input)
}

fn label_op(input: &str) -> nom::IResult<&str, Stmt> {
    let colon = tag(":");

    let (input, (label_id, _, _, _)) = tuple((label, space0, colon, multispace0))(input)?;
    let label_id = label_id.to_string();

    Ok((input, Stmt::Label(label_id)))
}

fn jmp_op(input: &str) -> nom::IResult<&str, Stmt> {
    let op = tag("jmp");
    let (input, (_, true_branch)) = separated_pair(op, multispace1, label)(input)?;
    let true_branch = Target::Name(true_branch.to_string());

    let separator = tag(",");
    let (input, result) = opt(tuple((
        multispace0,
        separator,
        multispace0,
        label,
        multispace0,
    )))(input)?;
    let false_branch = match result {
        Some((_, _, _, name, _)) => Target::Name(name.to_string()),
        None => Target::NextAddress,
    };

    Ok((input, Stmt::Jmp(true_branch, false_branch)))
}

fn set_op(input: &str) -> nom::IResult<&str, Stmt> {
    let op = tag("+");
    let (input, (_, _)) = pair(op, multispace0)(input)?;

    Ok((input, Stmt::Write(WriteOp::Set)))
}

fn unset_op(input: &str) -> nom::IResult<&str, Stmt> {
    let op = tag("-");
    let (input, (_, _)) = tuple((op, multispace0))(input)?;

    Ok((input, Stmt::Write(WriteOp::Unset)))
}

fn seek_left_op(input: &str) -> nom::IResult<&str, Stmt> {
    let op = tag("<");
    let (input, (_, _)) = tuple((op, multispace0))(input)?;

    Ok((input, Stmt::Seek(SeekOp::Left(1))))
}

fn seek_left(input: &str) -> nom::IResult<&str, Stmt> {
    // XXX This makes some assumptions about what seek_{left,right}_op returns.
    // It would be better to pattern match Stmt::Seek(SeekOp::_(n)) and add
    // that to our accumulator.
    let (input, count) = fold_many1(seek_left_op, 0, |acc, _: Stmt| acc + 1)(input)?;

    Ok((input, Stmt::Seek(SeekOp::Left(count))))
}

fn seek_right_op(input: &str) -> nom::IResult<&str, Stmt> {
    let op = tag(">");
    let (input, (_, _)) = tuple((op, multispace0))(input)?;

    Ok((input, Stmt::Seek(SeekOp::Right(1))))
}

fn seek_right(input: &str) -> nom::IResult<&str, Stmt> {
    // XXX This makes some assumptions about what seek_{left,right}_op returns.
    // It would be better to pattern match Stmt::Seek(SeekOp::_(n)) and add
    // that to our accumulator.
    let (input, count) = fold_many1(seek_right_op, 0, |acc, _: Stmt| acc + 1)(input)?;

    Ok((input, Stmt::Seek(SeekOp::Right(count))))
}

fn input_op(input: &str) -> nom::IResult<&str, Stmt> {
    let op = tag(",");
    let (input, (_, _)) = tuple((op, multispace0))(input)?;

    Ok((input, Stmt::Io(IoOp::In)))
}

fn output_op(input: &str) -> nom::IResult<&str, Stmt> {
    let op = tag(".");
    let (input, (_, _)) = tuple((op, multispace0))(input)?;

    Ok((input, Stmt::Io(IoOp::Out)))
}

fn debug_op(input: &str) -> nom::IResult<&str, Stmt> {
    let op = tag("!");
    let (input, (_, _)) = tuple((op, multispace0))(input)?;

    Ok((input, Stmt::Debug))
}

fn statement(input: &str) -> nom::IResult<&str, Stmt> {
    alt((
        label_op, jmp_op, set_op, unset_op, seek_left, seek_right, input_op, output_op, debug_op,
    ))(input)
}

fn comment(input: &str) -> nom::IResult<&str, ()> {
    let (input, _) = tuple((tag("/*"), is_not("*/"), tag("*/"), multispace0))(input)?;

    Ok((input, ()))
}

fn any_statement(input: &str) -> nom::IResult<&str, Stmt> {
    // XXX Yeah, you can't put a comment anywhere. I am willing to live with that for the time
    // being
    let (input, _) = opt(comment)(input)?;
    let (input, _) = multispace0(input)?;

    let (input, stmt) = statement(input)?;

    let (input, _) = opt(comment)(input)?;
    let (input, _) = multispace0(input)?;

    Ok((input, stmt))
}

fn parse_entry(input: &str) -> nom::IResult<&str, Vec<Stmt>> {
    many0(any_statement)(input)
}

impl Program {
    fn parse_statements(unparsed: &str) -> Result<Vec<Stmt>, WmachErr> {
        let (rest, statements) = parse_entry(unparsed).map_err(|e| WmachErr::GeneralError {
            message: format!("Nom Error: {}", e),
        })?;

        let rest = String::from_utf8(rest.as_bytes().to_vec()).expect("Invalid UTF8");
        if rest.len() > 0 {
            Err(WmachErr::GeneralError {
                message: format!("Left over data: {}", rest),
            })?;
        }

        Ok(statements)
    }

    pub fn from_file(filename: &Path) -> Result<Program, WmachErr> {
        let mut unparsed_file = String::new();
        File::open(filename)?.read_to_string(&mut unparsed_file)?;

        Program::from_str(&unparsed_file)
    }
}

#[cfg(test)]
mod constraint_tests {
    use super::*;

    #[test]
    fn parse_label() {
        let name = "my_label";
        let program = format!("{}:", name);
        let result = label_op(&program);

        let (_, id) = match result {
            Ok(whatever) => whatever,
            _ => panic!("parse failed: {:?}", result),
        };

        let id = match id {
            Stmt::Label(id) => id,
            _ => panic!("wrong variant"),
        };
        assert_eq!(id, name);
    }

    /*
    #[test]
    fn stacked_label() {
        let program = "alias0: alias1:";
        let result = Program::from_str(&program).expect("should parse fine");

        // This is kind of insane... Basically we want to iterate over each element and ensure
        // they're all identical.
        let value = result.labels.values().fold(Ok(None), |acc, offset| {
            if acc.is_ok() {
                let inner = acc.unwrap();
                if inner.is_none() {
                    Ok(Some(offset))
                } else if inner != Some(offset) {
                    Err(())
                } else {
                    acc
                }
            } else {
                acc
            }
        });
        assert!(value.is_ok());
    }
    */

    #[test]
    fn parse_jmp_single() {
        let true_branch = "first";
        let program = format!("jmp {}", true_branch);
        let result = jmp_op(&program);

        let (_, jmp) = match result {
            Ok(whatever) => whatever,
            _ => panic!("parse failed: {:?}", result),
        };

        let (branch_a, branch_b) = match jmp {
            Stmt::Jmp(branch_a, branch_b) => (branch_a, branch_b),
            _ => panic!("parsed stmt incorrect: {:?}", jmp),
        };

        match branch_a {
            Target::Name(name) => assert_eq!(name, true_branch),
            _ => panic!("parsed frist branch incorrectly: {:?}", branch_a),
        };
        match branch_b {
            Target::NextAddress => assert!(true),
            _ => panic!("parsed second branch incorrectly: {:?}", branch_b),
        };
    }

    #[test]
    fn parse_jmp_double() {
        let true_branch = "first";
        let false_branch = "first";
        let program = format!("jmp {}, {}", true_branch, false_branch);
        let result = jmp_op(&program);

        let (_, jmp) = match result {
            Ok(whatever) => whatever,
            _ => panic!("parse failed: {:?}", result),
        };

        let (branch_a, branch_b) = match jmp {
            Stmt::Jmp(branch_a, branch_b) => (branch_a, branch_b),
            _ => panic!("parsed stmt incorrect: {:?}", jmp),
        };

        match branch_a {
            Target::Name(name) => assert_eq!(name, true_branch),
            _ => panic!("parsed frist branch incorrectly: {:?}", branch_a),
        };
        match branch_b {
            Target::Name(name) => assert_eq!(name, false_branch),
            _ => panic!("parsed second branch incorrectly: {:?}", branch_b),
        };
    }

    #[test]
    fn parse_set() {
        let program = format!("+");
        let result = set_op(&program);

        let (_, set) = match result {
            Ok(whatever) => whatever,
            _ => panic!("parse failed: {:?}", result),
        };

        let op = match set {
            Stmt::Write(op) => op,
            _ => panic!("parsed stmt incorect: {:?}", set),
        };

        match op {
            WriteOp::Set => assert!(true),
            _ => panic!("invalid op: {:?}", op),
        };
    }

    #[test]
    fn parse_unset() {
        let program = format!("-");
        let result = unset_op(&program);

        let (_, unset) = match result {
            Ok(whatever) => whatever,
            _ => panic!("parse failed: {:?}", result),
        };

        let op = match unset {
            Stmt::Write(op) => op,
            _ => panic!("parsed stmt incorect: {:?}", unset),
        };

        match op {
            WriteOp::Unset => assert!(true),
            _ => panic!("invalid op: {:?}", op),
        };
    }

    #[test]
    fn parse_seek_left() {
        let program = format!("<");
        let result = seek_left_op(&program);

        let (_, stmt) = match result {
            Ok(whatever) => whatever,
            _ => panic!("parse failed: {:?}", result),
        };

        let op = match stmt {
            Stmt::Seek(op) => op,
            _ => panic!("parsed stmt incorect: {:?}", stmt),
        };

        match op {
            SeekOp::Left(1) => assert!(true),
            _ => panic!("invalid op: {:?}", op),
        };
    }

    #[test]
    fn parse_seek_right() {
        let program = format!(">");
        let result = seek_right_op(&program);

        let (_, stmt) = match result {
            Ok(whatever) => whatever,
            _ => panic!("parse failed: {:?}", result),
        };

        let op = match stmt {
            Stmt::Seek(op) => op,
            _ => panic!("parsed stmt incorect: {:?}", stmt),
        };

        match op {
            SeekOp::Right(1) => assert!(true),
            _ => panic!("invalid op: {:?}", op),
        };
    }

    #[test]
    fn parse_input() {
        let program = format!(",");
        let result = input_op(&program);

        let (_, stmt) = match result {
            Ok(whatever) => whatever,
            _ => panic!("parse failed: {:?}", result),
        };

        let op = match stmt {
            Stmt::Io(op) => op,
            _ => panic!("parsed stmt incorect: {:?}", stmt),
        };

        match op {
            IoOp::In => assert!(true),
            _ => panic!("invalid op: {:?}", op),
        };
    }

    #[test]
    fn parse_output() {
        let program = format!(".");
        let result = output_op(&program);

        let (_, stmt) = match result {
            Ok(whatever) => whatever,
            _ => panic!("parse failed: {:?}", result),
        };

        let op = match stmt {
            Stmt::Io(op) => op,
            _ => panic!("parsed stmt incorect: {:?}", stmt),
        };

        match op {
            IoOp::Out => assert!(true),
            _ => panic!("invalid op: {:?}", op),
        };
    }

    #[test]
    fn parse_debug() {
        let program = format!("!");
        let result = debug_op(&program);

        let (_, stmt) = match result {
            Ok(whatever) => whatever,
            _ => panic!("parse failed: {:?}", result),
        };

        match stmt {
            Stmt::Debug => assert!(true),
            _ => panic!("parsed stmt incorect: {:?}", stmt),
        };
    }

    /*
    #[test]
    fn parse_statement() {
        for program in [] {
        }
    }
    */

    // TODO finish tests
}
