"""Explicit approval only for existing disposable test workflows."""
from pathlib import Path
import tempfile
from unittest import mock
import change_review as review


def approve_fixture(test,owner):
    temporary=tempfile.TemporaryDirectory(prefix='auriga-approved-test-')
    test.addCleanup(temporary.cleanup)
    owner.change_review_root=Path(temporary.name)
    def approve(owner,title,kind,payload,rows,files=None):
        owner.change_review_root=Path(temporary.name)
        stage=review.Stage.create(owner,title,kind,payload,rows,files);stage.approve(owner);return stage
    patch=mock.patch.object(review,'request',side_effect=approve)
    patch.start();test.addCleanup(patch.stop)
