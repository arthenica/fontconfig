extern crate fc_fontations_bindgen;

use std::ffi::CString;
use std::fmt::Debug;

use fc_fontations_bindgen::fcint::{
    FcPattern, FcPatternCreate, FcPatternDestroy, FcPatternObjectAddBool, FcPatternObjectAddDouble,
    FcPatternObjectAddInteger, FcPatternObjectAddRange, FcPatternObjectAddString, FcRange,
    FcRangeCopy, FcRangeCreateDouble, FcRangeDestroy, FC_FAMILY_OBJECT,
};

#[allow(unused)]
#[derive(Debug)]
pub struct FcRangeWrapper {
    range: *mut FcRange,
}

impl FcRangeWrapper {
    #[allow(unused)]
    pub fn from_raw(range: *mut FcRange) -> Self {
        Self { range }
    }

    #[allow(unused)]
    pub fn new(min: f64, max: f64) -> Option<Self> {
        let created = unsafe { FcRangeCreateDouble(min, max) };
        if created.is_null() {
            None
        } else {
            Some(Self { range: created })
        }
    }

    #[allow(dead_code)]
    pub fn into_raw(mut self) -> *mut FcRange {
        let return_range = self.range;
        self.range = std::ptr::null_mut();
        return_range
    }
}

impl Clone for FcRangeWrapper {
    fn clone(&self) -> Self {
        Self {
            range: unsafe { FcRangeCopy(self.range) },
        }
    }
}

impl Drop for FcRangeWrapper {
    fn drop(&mut self) {
        unsafe {
            FcRangeDestroy(self.range);
        }
    }
}

pub struct FcPatternWrapper {
    pattern: *mut FcPattern,
}

impl FcPatternWrapper {
    pub fn new() -> Option<Self> {
        unsafe {
            let pattern = FcPatternCreate();
            if pattern.is_null() {
                None
            } else {
                Some(Self { pattern })
            }
        }
    }

    pub fn as_ptr(&self) -> *mut FcPattern {
        assert!(!self.pattern.is_null());
        self.pattern
    }

    #[allow(unused)]
    pub fn into_raw(mut self) -> *mut FcPattern {
        let return_pattern = self.pattern;
        self.pattern = std::ptr::null_mut();
        return_pattern
    }
}

impl Drop for FcPatternWrapper {
    fn drop(&mut self) {
        unsafe {
            FcPatternDestroy(self.pattern);
        }
    }
}

#[derive(Debug, Clone)]
pub struct PatternString {
    pub fontconfig_id: i32,
    pub string: CString,
}

impl PatternString {
    #[allow(unused)]
    pub fn new(fontconfig_id: i32, string: CString) -> Self {
        Self {
            fontconfig_id,
            string,
        }
    }
}

#[allow(unused)]
#[derive(Debug, Clone)]
pub enum PatternElement {
    String(PatternString),
    Boolean(i32, bool),
    Integer(i32, i32),
    Double(i32, f64),
    Range(i32, FcRangeWrapper),
}

#[derive(Debug, Clone)]
struct PatternAddError;

impl std::fmt::Display for PatternAddError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "Failed to add object to Fontconfig pattern.")
    }
}

impl PatternElement {
    fn append_to_fc_pattern(self, pattern: *mut FcPattern) -> Result<(), PatternAddError> {
        let pattern_add_success = match self {
            PatternElement::String(string) => unsafe {
                FcPatternObjectAddString(
                    pattern,
                    string.fontconfig_id,
                    string.string.as_ptr() as *const u8,
                )
            },
            PatternElement::Boolean(id, value) => unsafe {
                FcPatternObjectAddBool(pattern, id, value as i32)
            },
            PatternElement::Integer(id, value) => unsafe {
                FcPatternObjectAddInteger(pattern, id, value)
            },
            PatternElement::Double(id, value) => unsafe {
                FcPatternObjectAddDouble(pattern, id, value)
            },
            PatternElement::Range(id, value) => unsafe {
                FcPatternObjectAddRange(pattern, id, value.into_raw())
            },
        } == 1;
        if pattern_add_success {
            return Ok(());
        }
        Err(PatternAddError)
    }
}

#[derive(Default, Debug, Clone)]
pub struct FcPatternBuilder {
    elements: Vec<PatternElement>,
}

impl FcPatternBuilder {
    #[allow(unused)]
    pub fn new() -> Self {
        Self::default()
    }

    #[allow(unused)]
    pub fn append_element(&mut self, element: PatternElement) {
        self.elements.push(element);
    }

    #[allow(unused)]
    pub fn create_fc_pattern(&mut self) -> Option<FcPatternWrapper> {
        let pattern = FcPatternWrapper::new()?;

        let mut family_name_encountered = false;

        const FAMILY_ID: i32 = FC_FAMILY_OBJECT as i32;
        for element in self.elements.drain(0..) {
            if let PatternElement::String(PatternString {
                fontconfig_id: FAMILY_ID,
                string: ref fam_name,
            }) = element
            {
                if !fam_name.is_empty() {
                    family_name_encountered = true;
                }
            }
            element.append_to_fc_pattern(pattern.as_ptr()).ok()?;
        }

        if !family_name_encountered {
            return None;
        }

        Some(pattern)
    }
}

#[cfg(test)]
mod test {
    use std::ffi::CString;

    use super::{FcPatternBuilder, FcRangeWrapper, PatternElement, PatternString};
    use fc_fontations_bindgen::fcint::{
        FcPatternObjectGetBool, FcPatternObjectGetDouble, FcPatternObjectGetInteger,
        FcPatternObjectGetRange, FcPatternObjectGetString, FcRange, FC_COLOR_OBJECT,
        FC_FAMILY_OBJECT, FC_SLANT_OBJECT, FC_WEIGHT_OBJECT, FC_WIDTH_OBJECT,
    };

    #[test]
    fn verify_pattern_bindings() {
        let mut pattern_builder = FcPatternBuilder::new();

        // Add a bunch of test properties.
        pattern_builder.append_element(PatternElement::Boolean(FC_COLOR_OBJECT as i32, true));
        pattern_builder.append_element(PatternElement::Double(FC_WEIGHT_OBJECT as i32, 800.));

        pattern_builder.append_element(PatternElement::Integer(FC_SLANT_OBJECT as i32, 15));

        pattern_builder.append_element(PatternElement::Range(
            FC_WIDTH_OBJECT as i32,
            FcRangeWrapper::new(100., 400.).unwrap(),
        ));

        pattern_builder.append_element(PatternElement::String(PatternString {
            fontconfig_id: FC_FAMILY_OBJECT as i32,
            string: CString::new("TestFont").unwrap(),
        }));

        let pattern = pattern_builder.create_fc_pattern().unwrap();

        let fontconfig_pattern = pattern.as_ptr();
        unsafe {
            // Verify color properties.
            let mut result: i32 = 0;
            let get_result =
                FcPatternObjectGetBool(fontconfig_pattern, FC_COLOR_OBJECT as i32, 0, &mut result);
            assert_eq!(get_result, 0);
            assert_eq!(result, 1);

            // Verify weight value.
            let mut weight_result: f64 = 0.;
            let get_result = FcPatternObjectGetDouble(
                fontconfig_pattern,
                FC_WEIGHT_OBJECT as i32,
                0,
                &mut weight_result,
            );
            assert_eq!(get_result, 0);
            assert_eq!(weight_result, 800.0);

            // Verify that weight is not a range.
            let range_result: *mut *mut FcRange = std::mem::zeroed();
            assert_eq!(
                FcPatternObjectGetRange(
                    fontconfig_pattern,
                    FC_WEIGHT_OBJECT as i32,
                    0,
                    range_result
                ),
                2
            );

            // Verify slant.
            let mut slant_result: i32 = 0;
            let get_result = FcPatternObjectGetInteger(
                fontconfig_pattern,
                FC_SLANT_OBJECT as i32,
                0,
                &mut slant_result,
            );
            assert_eq!(get_result, 0);
            assert_eq!(slant_result, 15);

            // Verify width.
            let mut width_result: *mut FcRange = std::mem::zeroed();
            let get_result = FcPatternObjectGetRange(
                fontconfig_pattern,
                FC_WIDTH_OBJECT as i32,
                0,
                &mut width_result,
            );
            assert_eq!(get_result, 0);
            assert_eq!((*width_result).begin, 100.);
            assert_eq!((*width_result).end, 400.);

            // Verify family name.
            let mut family_result: *mut u8 = std::mem::zeroed();
            let get_result = FcPatternObjectGetString(
                fontconfig_pattern,
                FC_FAMILY_OBJECT as i32,
                0,
                &mut family_result,
            );
            assert_eq!(get_result, 0);
            assert_eq!(
                std::ffi::CStr::from_ptr(family_result as *const i8)
                    .to_str()
                    .unwrap(),
                "TestFont"
            );
        }
    }
}
